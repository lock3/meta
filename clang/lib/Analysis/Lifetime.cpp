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

  std::map<const Expr *, PSet> PSetsOfExpr;
  std::map<const Expr *, PSet> RefersTo;

  bool computeEntryPSets(const CFGBlock &B);

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

  /// Approximate the SourceLocation of a Block for attaching pset debug
  /// diagnostics.
  SourceLocation getStartLocOfBlock(const CFGBlock &B) const {
    if (&B == &ControlFlowGraph->getEntry())
      return FuncDecl->getBeginLoc();

    if (&B == &ControlFlowGraph->getExit())
      return FuncDecl->getEndLoc();

    for (const CFGElement &E : B) {
      switch (E.getKind()) {
      case CFGElement::Statement:
        return E.castAs<CFGStmt>().getStmt()->getBeginLoc();
      case CFGElement::LifetimeEnds:
        return E.castAs<CFGLifetimeEnds>().getTriggerStmt()->getEndLoc();
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

  void dumpCFG() const { ControlFlowGraph->dump(ASTCtxt.getLangOpts(), true); }
};

static void mergePMaps(PSetsMap &From, PSetsMap &To) {
  for (auto &I : From) {
    auto &Var = I.first;
    auto &PS = I.second;
    auto J = To.find(Var);
    if (J == To.end())
      To.insert(I);
    else
      J->second.merge(PS);
  }
}

/// Computes entry psets of this block by merging exit psets
/// of all reachable predecessors.
/// Returns true if this block is reachable, i.e. one of it predecessors has
/// been visited.
bool LifetimeContext::computeEntryPSets(const CFGBlock &B) {
  // Some special blocks has no real instructions but they merge the PMaps
  // prematurely at some emrge points. Try to keep the false/true PMaps
  // separate at those merge points to retain as much infromation as possible.
  auto &BC = getBlockContext(&B);
  bool PropagateFalseSet = false;
  if (B.succ_size() == 2 && isNoopBlock(B) &&
      llvm::all_of(B.preds(), [this](const CFGBlock::AdjacentBlock &Pred) {
        return Pred && getBlockContext(Pred).FalseBranchExitPMap;
      })) {
    PropagateFalseSet = true;
    BC.FalseBranchExitPMap = PSetsMap();
  }

  // If no predecessors have been visited by now, this block is not reachable.
  bool IsReachable = false;
  for (auto &PredBlock : B.preds()) {
    if (!PredBlock.isReachable())
      continue;

    auto &PredBC = getBlockContext(PredBlock);
    if (!PredBC.Visited)
      continue; // Skip this back edge.

    IsReachable = true;
    if (PropagateFalseSet) {
      // Predecessor have different PSets for true and false branches.
      // Figure out which PSets should be propated.
      if (PredBlock->succ_size() == 2) {
        // Is this a true or a false edge?
        if (*PredBlock->succ_rbegin() == &B) {
          mergePMaps(*PredBC.FalseBranchExitPMap, *BC.FalseBranchExitPMap);
        } else
          mergePMaps(PredBC.ExitPMap, BC.EntryPMap);
      } else if (PredBlock->succ_size() == 1) {
        // We only have one edge. Propagate both sets.
        mergePMaps(*PredBC.FalseBranchExitPMap, *BC.FalseBranchExitPMap);
        mergePMaps(PredBC.ExitPMap, BC.EntryPMap);
      }
    } else {
      auto &PredPMap =
          (PredBlock->succ_size() == 2 && *PredBlock->succ_rbegin() == &B &&
           PredBC.FalseBranchExitPMap)
              ? *PredBC.FalseBranchExitPMap
              : PredBC.ExitPMap;
      mergePMaps(PredPMap, BC.EntryPMap);
    }
  }

  return IsReachable;
}

/// Initialize psets for all members of *this that are Owner or Pointers.
/// Assume that all Pointers are valid.
static void createEntryPsetsForMembers(const CXXMethodDecl *Method, PSetsMap &PMap) {
  const CXXRecordDecl *RD = Method->getParent();
  auto CallBack = [&PMap, RD](const CXXRecordDecl *Base) {
    for (const FieldDecl *Field : Base->fields()) {
      switch (classifyTypeCategory(Field->getType())) {
      case TypeCategory::Pointer:
      case TypeCategory::Owner: {
        // Each Owner/Pointer points to deref of itself.
        Variable V = Variable::thisPointer(RD).deref();
        V.addFieldRef(Field);
        Variable DerefV = V;
        DerefV.deref();
        PMap[V] = PSet::singleton(DerefV);
        break;
      }
      default:
        break;
      }
    }
    return true;
  };

  CallBack(RD);
  RD->forallBases(CallBack);
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
        getLifetimeContracts(BC.ExitPMap, FuncDecl, ASTCtxt, IsConvertible,
                             Reporter);
        if (const auto *Method = dyn_cast<CXXMethodDecl>(FuncDecl))
          createEntryPsetsForMembers(Method, BC.ExitPMap);

        BC.Visited = true;
        continue;
      }

      if (B == &ControlFlowGraph->getExit())
        continue;

      // Compute entry psets of this block by merging exit psets of all
      // reachable predecessors.
      auto OrigEntryPMap = BC.EntryPMap;
      bool isReachableAndChanged = computeEntryPSets(*B);
      if (!isReachableAndChanged)
        continue;

      if (BC.Visited && OrigEntryPMap == BC.EntryPMap) {
        // Has been computed at least once and nothing changed; no need to
        // recompute.
        continue;
      }

      BC.ExitPMap = BC.EntryPMap;
      if (!VisitBlock(FuncDecl, BC.ExitPMap, BC.FalseBranchExitPMap,
                      PSetsOfExpr, RefersTo, *B, Reporter, ASTCtxt,
                      IsConvertible)) {
        // An unsupported AST node (such as reinterpret_cast) disabled
        // the analysis.
        return;
      }
      BC.Visited = true;
      Updated = true;
    }
    ++IterationCount;
  } while (Updated && IterationCount < IterationLimit);

  if (IterationCount > MaxIterations)
    MaxIterations = IterationCount;
}

bool isNoopBlock(const CFGBlock &B) {
  for (const auto &E : B) {
    switch (E.getKind()) {
    case CFGElement::Statement: {
      const Stmt *S = E.castAs<CFGStmt>().getStmt();
      if (const auto *E = dyn_cast<Expr>(S)) {
        if (!E->getType()->isBooleanType())
          return false;
        if (isa<CastExpr>(E))
          continue;
        if (const auto *BO = dyn_cast<BinaryOperator>(E))
          if (BO->isLogicalOp())
            continue;
      }
      break;
    }
    default:
      return false;
    }
  }
  return true;
}

static bool shouldSuppressLifetime(const FunctionDecl *Func) {
  auto Attr = Func->getAttr<SuppressAttr>();
  if (!Attr) {
    return false;
  }
  return llvm::any_of(Attr->diagnosticIdentifiers(), [](auto &Identifier) {
    return Identifier == "lifetime";
  });
}

/// Check that the function adheres to the lifetime profile.
void runAnalysis(const FunctionDecl *Func, ASTContext &Context,
                 LifetimeReporterBase &Reporter,
                 IsConvertibleTy IsConvertible) {

  if (!Func->doesThisDeclarationHaveABody())
    return;
  if (Func->isInStdNamespace())
    return;
  if (shouldSuppressLifetime(Func))
    return;
  if (const auto *DC = Func->getEnclosingNamespaceContext())
    if (const auto *NS = dyn_cast<NamespaceDecl>(DC))
      if (NS->getIdentifier() && NS->getName() == "gsl")
        return;
  if (!Func->getCanonicalDecl()->hasAttr<LifetimeContractAttr>())
    return;

  if (auto *M = dyn_cast<CXXMethodDecl>(Func)) {
    // Do not check the bodies of methods on Owners.
    auto Class = classifyTypeCategory(M->getParent()->getTypeForDecl());
    if (Class.TC == TypeCategory::Owner)
      return;
  }

  LifetimeContext LC(Context, Reporter, Func, IsConvertible);
  LC.TraverseBlocks();
}
} // namespace lifetime
} // namespace clang
