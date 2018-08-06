//=- Lifetime.cpp - Diagnose lifetime violations -*- C++ -*-==================//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// The pset of a (pointer/reference) variable can be modified by
//   1) Initialization
//   2) Assignment
// It will be set to the pset of the expression on the right-hand-side.
// Such expressions can contain:
//   1) Casts: Ignored, pset(expr) == pset((cast)expr)
//   2) Address-Of operator:
//        It can only be applied to lvalues, i.e.
//        VarDecl, pset(&a) = {a}
//        a function returning a ref,
//        result of an (compound) assignment, pset(&(a = b)) == {b}
//        pre-in/decrement, pset(&(--a)) = {a}
//        deref,
//        array subscript, pset(&a[3]) = {a}
//        a.b, a.*b: pset(&a.b) = {a}
//        a->b, a->*b,
//        comma expr, pset(&(a,b)) = {b}
//        string literal, pset(&"string") = {static}
//        static_cast<int&>(x)
//
//   3) Dereference operator
//   4) Function calls and CXXMemberCallExpr
//   5) MemberExpr: pset(this->a) = {a}; pset_ref(o->a) = {o}; pset_ptr(o->a) =
//   {o'}
//   6) Ternary: pset(cond ? a : b) == pset(a) union pset(b)
//   7) Assignment operator: pset(a = b) == {b}
// Rules:
//  1) T& p1 = expr; T* p2 = &expr; -> pset(p1) == pset(p2) == pset_ref(expr)
//  2) T& p1 = *expr; T* p2 = expr; -> pset(p1) == pset(p2) == pset_ptr(expr)
//  3) Casts are ignored: pset(expr) == pset((cast)expr)
//  4) T* p1 = &C.m; -> pset(p1) == {C} (per ex. 1.3)
//  5) T* p2 = C.get(); -> pset(p2) == {C'} (per ex. 8.1)
//
// Assumptions:
// - The 'this' pointer cannot be invalidated inside a member method (enforced
// here: no delete on *this)
// - Global variable's pset is (static) and/or (null) (enforced here)
// - Arithmetic on pointer types is forbidden (enforced by
// cppcoreguidelines-pro-type-pointer-arithmetic)
// - A function does not modify const arguments (enforced by
// cppcoreguidelines-pro-type-pointer-arithmetic)
// - An access to an array through array subscription always points inside the
// array (enforced by cppcoreguidelines-pro-bounds)
//
// TODO:
//  track psets for objects containing Pointers (e.g. iterators)
//  handle function/method call in PSetFromExpr
//  check correct use of gsl::owner<> (delete only on 'valid' owner, delete must
//  happen before owner goes out of scope)
//  handle try-catch blocks (AC.getCFGBuildOptions().AddEHEdges = true)
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
    BlockContexts.resize(ControlFlowGraph->getNumBlockIDs());
  }

  void TraverseBlocks();
};

static const Stmt *getRealTerminator(const CFGBlock *B) {
  const Stmt *LastCFGStmt = nullptr;
  for (const CFGElement &Element : *B) {
    if (auto CFGSt = Element.getAs<CFGStmt>()) {
      LastCFGStmt = CFGSt->getStmt();
    }
  }
  return LastCFGStmt;
}

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
    auto PredPSets = PredBC.ExitPSets;
    // Unfortunately, PredBlock->getTerminatorCondition(true) is almost what
    // we whant here but not quite. In case of A || B, for the basic block
    // corresponding to B, the terminator expression is the whole
    // A || B. Is this a bug?
    if (auto TermCond = getRealTerminator(PredBlock)) {
      // First successor is the then-branch, second successor is the
      // else-branch.
      bool IsThenBranch = PredBlock->succ_begin()->getReachableBlock() == &B;
      UpdatePSetsFromCondition(PredPSets, &Reporter, ASTCtxt, TermCond,
                               IsThenBranch, TermCond->getLocStart());
    }
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
          if (classifyTypeCategory(PVD->getType()) != TypeCategory::Pointer)
            continue;
          Variable P(PVD);
          // Parameters cannot be invalid (checked at call site).
          auto PS = PSet::pointsToVariable(P, P.mightBeNull(), 0);
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
      VisitBlock(BC.ExitPSets, *B, /*Reporter=*/nullptr, ASTCtxt);
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
    VisitBlock(BC.ExitPSets, *B, &Reporter, ASTCtxt);
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

/// Check that each global variable is initialized to a pset of {static}
/// and/or {null}
void runAnalysis(const VarDecl *VD, ASTContext &Context,
                 LifetimeReporterBase &Reporter) {

  if (classifyTypeCategory(VD->getType()) != TypeCategory::Pointer)
    return;

  PSetsMap PSets;
  EvalVarDecl(PSets, VD, &Reporter, Context);
  // TODO
  // We don't track the PSets of variables with global storage; just make
  // sure that its pset is always {static} and/or {null}
  // if (!PS.isSubsetOf(PSet::validOwnerOrNull(Owner::Static())) && Reporter)
  // Reporter->warnPsetOfGlobal(Loc, P->getName(), PS.str());
}

} // namespace lifetime
} // namespace clang
