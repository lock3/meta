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
#include "clang/Analysis/Analyses/LifetimePsetBuilder.h"
#include "clang/Analysis/Analyses/PostOrderCFGView.h"
#include "clang/Analysis/CFG.h"
#include "clang/Analysis/CFGStmtMap.h"
#include "llvm/ADT/Statistic.h"

#define DEBUG_TYPE "Lifetime Analysis"

STATISTIC(MaxIterations, "The maximum # of passes over the cfg");

namespace clang {
namespace lifetime {

LookupOperatorTy GlobalLookupOperator;
LookupMemberFunctionTy GlobalLookupMemberFunction;
DefineClassTemplateSpecializationTy GlobalDefineClassTemplateSpecialization;

class LifetimeContext {
  /// Additional information for each CFGBlock.
  struct BlockContext {
    bool Visited = false;
    /// Merged PSets of all predecessors of this CFGBlock.
    PSetsMap EntryPMap;
    /// Computed PSets after updating EntryPSets through all CFGElements of
    /// this block.
    PSetsMap ExitPMap;
    /// For blocks representing a branch, we have different psets for
    /// the true and the false branch.
    llvm::Optional<PSetsMap> FalseBranchExitPMap;
  };

  ASTContext &ASTCtxt;
  CFG *ControlFlowGraph;
  const FunctionDecl *FuncDecl;
  std::vector<BlockContext> BlockContexts;
  AnalysisDeclContext AC;
  LifetimeReporterBase &Reporter;
  IsConvertibleTy IsConvertible;

  PSet PSetOfAllParams;
  std::map<const Expr *, PSet> PSetsOfExpr;
  std::map<const Expr *, PSet> RefersTo;

  bool computeEntryPSets(const CFGBlock &B, PSetsMap &EntryPMap);

  BlockContext &getBlockContext(const CFGBlock *B) {
    return BlockContexts[B->getBlockID()];
  }

  void dumpBlock(const CFGBlock &B) const {
    auto Loc = getStartLocOfBlock(B);
    if (Loc.isValid()) {
      auto &SourceMgr = ASTCtxt.getSourceManager();
      llvm::errs() << "Block at " << SourceMgr.getBufferName(Loc) << ":"
                   << SourceMgr.getSpellingLineNumber(Loc) << "\n";
    }
    B.dump(ControlFlowGraph, ASTCtxt.getLangOpts(), true);
  }

  void dumpCFG() const { ControlFlowGraph->dump(ASTCtxt.getLangOpts(), true); }

  /// Approximate the SourceLocation of a Block for attaching pset debug
  /// diagnostics.
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
    return {};
  }

public:
  LifetimeContext(ASTContext &ASTCtxt, LifetimeReporterBase &Reporter,
                  const FunctionDecl *FuncDecl, IsConvertibleTy IsConvertible)
      : ASTCtxt(ASTCtxt), FuncDecl(FuncDecl), AC(nullptr, FuncDecl),
        Reporter(Reporter), IsConvertible(IsConvertible) {
    AC.getCFGBuildOptions().PruneTriviallyFalseEdges = true;
    AC.getCFGBuildOptions().AddInitializers = true;
    AC.getCFGBuildOptions().AddLifetime = true;
    AC.getCFGBuildOptions().AddStaticInitBranches = true;
    AC.getCFGBuildOptions().AddCXXNewAllocator = true;
    AC.getCFGBuildOptions().AddExprWithCleanups = true;
    AC.getCFGBuildOptions().AddCXXDefaultInitExprInCtors = true;
    // TODO AddTemporaryDtors
    // TODO AddEHEdges
    AC.getCFGBuildOptions().setAllAlwaysAdd();
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
                                        PSetsMap &EntryPMap) {
  // If no predecessors have been visited by now, this block is not reachable.
  bool IsReachable = false;
  for (auto &PredBlock : B.preds()) {
    if (!PredBlock.isReachable())
      continue;

    auto &PredBC = getBlockContext(PredBlock);
    if (!PredBC.Visited)
      continue; // Skip this back edge.

    IsReachable = true;
    // Is this a true or a false branch from the predecessor? We have might
    // have different state for both.
    auto PredPSets =
        (PredBlock->succ_size() == 2 && *PredBlock->succ_rbegin() == &B &&
         PredBC.FalseBranchExitPMap)
            ? *PredBC.FalseBranchExitPMap
            : PredBC.ExitPMap;
    // Merge PSets with pred's PSets; TODO: make this efficient
    for (auto &I : PredPSets) {
      auto &Var = I.first;
      auto &PS = I.second;
      auto J = EntryPMap.find(Var);
      if (J == EntryPMap.end())
        EntryPMap.insert(I);
      else if (!(PS == J->second))
        J->second.merge(PS);
    }
  }
  return IsReachable;
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

      // The entry block introduces the function parameters into the psets.
      if (B == &ControlFlowGraph->getEntry()) {
        if (BC.Visited)
          continue;

        // ExitPSets are the function parameters.
        PSetOfAllParams = PopulatePSetForParams(BC.ExitPMap, FuncDecl);
        BC.Visited = true;
        continue;
      }

      if (B == &ControlFlowGraph->getExit())
        continue;

      // Compute entry psets of this block by merging exit psets of all
      // reachable predecessors.
      PSetsMap EntryPMap;
      bool isReachable = computeEntryPSets(*B, EntryPMap);
      if (!isReachable)
        continue;

      if (BC.Visited && EntryPMap == BC.EntryPMap) {
        // Has been computed at least once and nothing changed; no need to
        // recompute.
        continue;
      }

      BC.EntryPMap = EntryPMap;
      BC.ExitPMap = BC.EntryPMap;
      VisitBlock(BC.ExitPMap, BC.FalseBranchExitPMap, PSetOfAllParams,
                 PSetsOfExpr, RefersTo, *B, Reporter, ASTCtxt, IsConvertible);
      BC.Visited = true;
      Updated = true;
    }
    ++IterationCount;
  } while (Updated && IterationCount < IterationLimit);

  if (IterationCount > MaxIterations)
    MaxIterations = IterationCount;
}

/// Check that the function adheres to the lifetime profile
void runAnalysis(
    const FunctionDecl *Func, ASTContext &Context,
    LifetimeReporterBase &Reporter, IsConvertibleTy IsConvertible,
    LookupOperatorTy LookupOperator,
    LookupMemberFunctionTy LookupMemberFunction,
    DefineClassTemplateSpecializationTy DefineClassTemplateSpecialization) {
  if (!Func->doesThisDeclarationHaveABody())
    return;

  GlobalLookupOperator = LookupOperator;
  GlobalLookupMemberFunction = LookupMemberFunction;
  GlobalDefineClassTemplateSpecialization = DefineClassTemplateSpecialization;
  LifetimeContext LC(Context, Reporter, Func, IsConvertible);
  LC.TraverseBlocks();
}
} // namespace lifetime
} // namespace clang
