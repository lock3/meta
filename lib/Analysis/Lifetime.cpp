//=- Lifetime.cpp - Diagnose lifetime violations -*- C++ -*-==================//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#include "clang/Analysis/Analyses/Lifetime.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/Analysis/Analyses/LifetimePsetBuilder.h"
#include "clang/Analysis/Analyses/PostOrderCFGView.h"
#include "clang/Analysis/CFG.h"
#include "clang/Sema/SemaDiagnostic.h" // TODO: remove me and move all diagnostics into LifetimeReporter
#include "llvm/ADT/Statistic.h"
#include <algorithm>
#include <map>
#include <sstream>
#include <unordered_map>

#define DEBUG_TYPE "Lifetime Analysis"

STATISTIC(MaxIterations, "The maximum # of passes over the cfg");

namespace clang {
namespace lifetime {

class LifetimeContext {
  /// psets for a CFGBlock
  struct BlockContext {
    bool visited = false;
    /// Merged PSets of all predecessors of this CFGBlock
    PSetsMap EntryPSets;
    /// Computed PSets after updating EntryPSets through all CFGElements of
    /// this block
    PSetsMap ExitPSets;
    /// For blocks representing a branch, we have different psets for
    /// the true and the false branch.
    llvm::Optional<PSetsMap> FalseBranchExitPSets;
  };

  ASTContext &ASTCtxt;
  LangOptions LangOpts;
  SourceManager &SourceMgr;
  CFG *ControlFlowGraph;
  const FunctionDecl *FuncDecl;
  std::vector<BlockContext> BlockContexts;
  AnalysisDeclContextManager AnalysisDCMgr;
  AnalysisDeclContext AC;
  LifetimeReporterBase &Reporter;

  std::map<const Expr *, PSet> PSetsOfExpr;
  std::map<const Expr *, PSet> RefersTo;

  bool computeEntryPSets(const CFGBlock &B, PSetsMap &EntryPSets);

  BlockContext &getBlockContext(const CFGBlock *B) {
    return BlockContexts[B->getBlockID()];
  }

  void dumpBlock(const CFGBlock &B) const {
    auto Loc = getStartLocOfBlock(B);
    llvm::errs() << "Block at " << SourceMgr.getBufferName(Loc) << ":"
                 << SourceMgr.getSpellingLineNumber(Loc) << "\n";
    B.dump(ControlFlowGraph, LangOpts, true);
  }

  void dumpCFG() const { ControlFlowGraph->dump(LangOpts, true); }

  /// Approximate the SourceLocation of a Block for attaching pset debug
  /// diagnostics
  SourceLocation getStartLocOfBlock(const CFGBlock &B) const {
    if (&B == &ControlFlowGraph->getEntry())
      return FuncDecl->getLocStart();

    if (&B == &ControlFlowGraph->getExit())
      return FuncDecl->getLocEnd();

    for (const CFGElement &E : B) {
      switch (E.getKind()) {
      case CFGElement::Statement:
        return E.castAs<CFGStmt>().getStmt()->getLocStart();
      case CFGElement::LifetimeEnds:
        return E.castAs<CFGLifetimeEnds>().getTriggerStmt()->getLocEnd();
      default:;
      }
    }
    if (B.succ_empty())
      return SourceLocation();
    // for(auto i = B.succ_begin(); i != B.succ_end(); ++i)
    //{
    // TODO: this may lead to infinite recursion
    return getStartLocOfBlock(**B.succ_begin());
    //}
    llvm_unreachable("Could not determine start loc of CFGBlock");
  }

public:
  LifetimeContext(ASTContext &ASTCtxt, LifetimeReporterBase &Reporter,
                  SourceManager &SourceMgr, const FunctionDecl *FuncDecl)
      : ASTCtxt(ASTCtxt), LangOpts(ASTCtxt.getLangOpts()), SourceMgr(SourceMgr),
        FuncDecl(FuncDecl), AnalysisDCMgr(ASTCtxt),
        AC(&AnalysisDCMgr, FuncDecl), Reporter(Reporter) {
    // TODO: do not build own CFG here. Use the one from callee
    // AnalysisBasedWarnings::IssueWarnings
    AC.getCFGBuildOptions().PruneTriviallyFalseEdges = true;
    AC.getCFGBuildOptions().AddInitializers = true;
    AC.getCFGBuildOptions().AddLifetime = true;
    AC.getCFGBuildOptions().AddStaticInitBranches = true;
    AC.getCFGBuildOptions().AddCXXNewAllocator = true;
    // TODO AddTemporaryDtors
    // TODO AddEHEdges
    ControlFlowGraph = AC.getCFG();
    // dumpCFG();
    BlockContexts.resize(ControlFlowGraph->getNumBlockIDs());
  }

  void TraverseBlocks();
};

/// Computes entry psets of this block by merging exit psets
/// of all reachable predecessors.
/// Returns true if this block is reachable, i.e. one of it predecessors has
/// been visited.
bool LifetimeContext::computeEntryPSets(const CFGBlock &B,
                                        PSetsMap &EntryPSets) {
  // If no predecessors have been visited by now, this block is not
  // reachable
  bool isReachable = false;
  for (auto i = B.pred_begin(); i != B.pred_end(); ++i) {
    CFGBlock *PredBlock = i->getReachableBlock();
    if (!PredBlock)
      continue;

    auto &PredBC = getBlockContext(PredBlock);
    if (!PredBC.visited)
      continue; // Skip this back edge.

    isReachable = true;
    // Is this a true or a false branch from the predecessor? We have might
    // have different state for both.
    // TODO: how does this work with false branch pruning?
    auto PredPSets =
        (PredBlock->succ_size() == 2 && *PredBlock->succ_rbegin() == &B &&
         PredBC.FalseBranchExitPSets)
            ? *PredBC.FalseBranchExitPSets
            : PredBC.ExitPSets;
    if (EntryPSets.empty())
      EntryPSets = PredPSets;
    else {
      // Merge PSets with pred's PSets; TODO: make this efficient
      for (auto &i : EntryPSets) {
        auto &Var = i.first;
        auto &PS = i.second;
        auto j = PredPSets.find(Var);
        if (j == PredPSets.end()) {
          // The only reason that predecessors have PSets for different
          // variables is that some of them lazily added global variables
          // or member variables.
          // If a global pointer is not mentioned, its pset is implicitly
          // {(null), (static)}

          // OR there was a goto that stayed in the same scope but skipped
          // back over the initialization of this Pointer.
          // Then we don't care, because the variable will not be referenced
          // in the C++ code before it is declared.

          PS = PSet::staticVar(Var.mightBeNull());
          continue;
        }
        if (PS == j->second)
          continue;

        PS.merge(j->second);
      }
    }
  }
  return isReachable;
}

/// Traverse all blocks of the CFG.
/// The traversal is repeated until the psets come to a steady state.
void LifetimeContext::TraverseBlocks() {
  const PostOrderCFGView *SortedGraph = AC.getAnalysis<PostOrderCFGView>();
  static const unsigned IterationLimit = 128;

  bool Updated;
  unsigned IterationCount = 0;
  do {
    Updated = false;
    for (const auto *B : *SortedGraph) {
      auto &BC = getBlockContext(B);

      // The entry block introduces the function parameters into the psets
      if (B == &ControlFlowGraph->getEntry()) {
        if (BC.visited)
          continue;

        // ExitPSets are the function parameters.
        for (const ParmVarDecl *PVD : FuncDecl->parameters()) {
          TypeCategory TC = classifyTypeCategory(PVD->getType());
          if (TC != TypeCategory::Pointer && TC != TypeCategory::Owner)
            continue;
          Variable P(PVD);
          // Parameters cannot be invalid (checked at call site).
          auto PS =
              PSet::singleton(P, P.mightBeNull(), TC == TypeCategory::Owner);
          // Reporter.PsetDebug(PS, PVD->getLocEnd(), P.getValue());
          // PVD->dump();
          BC.ExitPSets.emplace(P, std::move(PS));
        }
        BC.visited = true;
        continue;
      }

      if (B == &ControlFlowGraph->getExit())
        continue;

      // compute entry psets of this block by merging exit psets
      // of all reachable predecessors.
      PSetsMap EntryPSets;
      bool isReachable = computeEntryPSets(*B, EntryPSets);
      if (!isReachable)
        continue;

      if (BC.visited && EntryPSets == BC.EntryPSets) {
        // Has been computed at least once and nothing changed; no need to
        // recompute.
        continue;
      }

      BC.EntryPSets = EntryPSets;
      BC.ExitPSets = BC.EntryPSets;
      VisitBlock(BC.ExitPSets, BC.FalseBranchExitPSets, PSetsOfExpr, RefersTo,
                 *B, /*Reporter=*/nullptr, ASTCtxt);
      BC.visited = true;
      Updated = true;
    }
    ++IterationCount;
  } while (Updated && IterationCount < IterationLimit);

  if (IterationCount > MaxIterations)
    MaxIterations = IterationCount;

  // Once more to emit diagnostics with final psets
  for (const auto *B : *SortedGraph) {
    auto &BC = getBlockContext(B);
    if (!BC.visited)
      continue;

    BC.ExitPSets = BC.EntryPSets;
    VisitBlock(BC.ExitPSets, BC.FalseBranchExitPSets, PSetsOfExpr, RefersTo, *B,
               &Reporter, ASTCtxt);
  }
}

/// Check that the function adheres to the lifetime profile
void runAnalysis(const FunctionDecl *Func, ASTContext &Context,
                 SourceManager &SourceMgr, LifetimeReporterBase &Reporter) {
  if (!Func->doesThisDeclarationHaveABody())
    return;

  LifetimeContext LC(Context, Reporter, SourceMgr, Func);
  LC.TraverseBlocks();
}
} // namespace lifetime
} // namespace clang
