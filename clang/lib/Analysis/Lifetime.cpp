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
#include "clang/AST/Attr.h"
#include "clang/Analysis/Analyses/LifetimePsetBuilder.h"
#include "clang/Analysis/CFG.h"
#include "clang/Analysis/FlowSensitive/DataflowWorklist.h"
#include "clang/Basic/SourceManager.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"

#define DEBUG_TYPE "Lifetime Analysis"

STATISTIC(MaxBlockVisitCount, "The maximum # of block visit count");
STATISTIC(BlockVisitCount, "The cummulative # times blocks are visited");

namespace clang {
namespace lifetime {

void LifetimeReporterBase::initializeFiltering(CFG *Cfg,
                                               IsCleaningBlockTy ICB) {
  this->Cfg = Cfg;
  PostDom.buildDominatorTree(Cfg);
  Dom.buildDominatorTree(Cfg);
  IsCleaningBlock = ICB;
}

namespace {

// Mark all nodes true in a path backward from From.
// Returns true if one of the nodes satisifes predicate P.
template <typename Pred>
bool markPathReachable(
    const CFGBlock *From,
    const llvm::DenseMap<const CFGBlock *, const CFGBlock *> &ParentMap,
    llvm::BitVector &ToMark, Pred P) {
  auto ParentIt = ParentMap.end();
  do {
    // Already propagated from different path.
    if (ToMark[From->getBlockID()])
      return false;
    ToMark[From->getBlockID()] = true;
    if (P(From))
      return true;
    ParentIt = ParentMap.find(From);
    if (ParentIt == ParentMap.end())
      break;
    From = ParentIt->second;
  } while (true);
  return false;
}

// Returns true if there is a simple path from Source to Dest touching a node
// that satisfies predicate P.
template <typename Pred>
bool hasPathThroughNodePred(const CFG &Cfg, const CFGBlock *Source,
                            const CFGBlock *Dest, const Pred &P) {
  SmallVector<const CFGBlock *, 32> WorkList;
  llvm::BitVector Visited(Cfg.getNumBlockIDs());
  llvm::BitVector CanReachDest(Cfg.getNumBlockIDs());
  llvm::DenseMap<const CFGBlock *, const CFGBlock *> ParentMap;

  CanReachDest[Dest->getBlockID()] = true;
  Visited[Source->getBlockID()] = true;
  WorkList.push_back(Source);

  while (!WorkList.empty()) {
    const CFGBlock *Current = WorkList.pop_back_val();
    for (const CFGBlock *Succ : Current->succs()) {
      // Ensure we only mark path through simple paths
      // and skip infeasible branches.
      if (!Succ || Succ == Source)
        continue;

      ParentMap.insert(std::make_pair(Succ, Current));
      if (Visited[Succ->getBlockID()] || Succ == Dest) {
        if (CanReachDest[Succ->getBlockID()])
          if (markPathReachable(Current, ParentMap, CanReachDest, P))
            return true;
        continue;
      }
      Visited[Succ->getBlockID()] = true;
      WorkList.push_back(Succ);
    }
  }
  return false;
}

} // namespace

bool LifetimeReporterBase::shouldBeFiltered(const CFGBlock *Source,
                                            const Variable *V) const {
  if (!shouldFilterWarnings())
    return false;

  if (!PostDom.dominates(Current, Source) && !Dom.dominates(Source, Current))
    return true;

  // Now we do know that the either the path between the Current and Source is
  // feasible or one of the blocks is dead.
  // Next step: do we clean nullness/invalidness between the two?

  // If there is a (simple, i.e. no nodes repeated) path from Source to Current
  // that has a cleaning block, filter the warning.
  if (hasPathThroughNodePred(
          *Cfg, Source, Current,
          [V, this](const CFGBlock *B) { return IsCleaningBlock(*B, V); }))
    return true;

  return false;
}

class LifetimeContext {
  /// Additional information for each CFGBlock.
  struct BlockContext {
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

  void computeEntryPSets(const CFGBlock &B);

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
    AC.getCFGBuildOptions().AddCXXDefaultInitExprInAggregates = true;
    // TODO AddTemporaryDtors
    // TODO AddEHEdges
    AC.getCFGBuildOptions().setAllAlwaysAdd();
    ControlFlowGraph = AC.getCFG();
    // dumpCFG();
    BlockContexts.resize(ControlFlowGraph->getNumBlockIDs());

    if (Reporter.shouldFilterWarnings()) {
      // Returns true if a block overwrites a variable that contains null or
      // invalid with something that is not null or invalid.
      auto IsCleaningBlock = [this](const CFGBlock &B, const Variable *V) {
        if (!V)
          return false;
        BlockContext &BCtx = getBlockContext(&B);
        auto PSetOfVarBefore = BCtx.EntryPMap.find(*V);
        auto PSetOfVarAfter = BCtx.ExitPMap.find(*V);
        if (PSetOfVarBefore == BCtx.EntryPMap.end())
          return false;
        assert(PSetOfVarAfter != BCtx.ExitPMap.end());
        if (!PSetOfVarBefore->second.containsNull() &&
            !PSetOfVarBefore->second.containsInvalid())
          return false;
        if (PSetOfVarAfter->second.containsNull() ||
            PSetOfVarAfter->second.containsInvalid())
          return false;
        return true;
      };
      Reporter.initializeFiltering(ControlFlowGraph, IsCleaningBlock);
    }
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
void LifetimeContext::computeEntryPSets(const CFGBlock &B) {
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

  for (auto &PredBlock : B.preds()) {
    if (!PredBlock.isReachable())
      continue;

    auto &PredBC = getBlockContext(PredBlock);

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
}

/// Initialize psets for all members of *this that are Owner or Pointers.
/// Assume that all Pointers are valid.
static void createEntryPsetsForMembers(const CXXMethodDecl *Method,
                                       PSetsMap &PMap) {
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
  static constexpr unsigned IterationLimit = 100000;

  ForwardDataflowWorklist WorkList(*ControlFlowGraph, AC);
  // The entry block introduces the function parameters into the psets.
  auto Start = &ControlFlowGraph->getEntry();
  auto &BC = getBlockContext(Start);
  // ExitPSets are the function parameters.
  getLifetimeContracts(BC.ExitPMap, FuncDecl, ASTCtxt, Start, IsConvertible,
                       Reporter);
  if (const auto *Method = dyn_cast<CXXMethodDecl>(FuncDecl))
    createEntryPsetsForMembers(Method, BC.ExitPMap);

  WorkList.enqueueSuccessors(Start);

  unsigned IterationCount = 0;
  llvm::BitVector Visited(ControlFlowGraph->getNumBlockIDs());
  const CFGBlock *Current;
  while ((Current = WorkList.dequeue()) && IterationCount < IterationLimit) {
    if (Current == &ControlFlowGraph->getExit())
      continue;

    auto &BC = getBlockContext(Current);

    // Compute entry psets of this block by merging exit psets of all
    // reachable predecessors.
    auto OrigEntryPMap = BC.EntryPMap;
    computeEntryPSets(*Current);
    if (Visited[Current->getBlockID()] && BC.EntryPMap == OrigEntryPMap) {
      // Has been computed at least once and nothing changed; no need to
      // recompute.
      continue;
    }

    ++BlockVisitCount;
    ++IterationCount;
    BC.ExitPMap = BC.EntryPMap;
    Visited[Current->getBlockID()] = true;
    if (!VisitBlock(FuncDecl, BC.ExitPMap, BC.FalseBranchExitPMap, PSetsOfExpr,
                    RefersTo, *Current, Reporter, ASTCtxt, IsConvertible)) {
      // An unsupported AST node (such as reinterpret_cast) disabled
      // the analysis.
      break;
    }
    WorkList.enqueueSuccessors(Current);
  }

  if (IterationCount > MaxBlockVisitCount)
    MaxBlockVisitCount = IterationCount;
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
      return false;
    }
    default:
      return false;
    }
  }
  return true;
}

static bool shouldSuppressLifetime(const FunctionDecl *Func) {
  const auto *Attr = Func->getAttr<SuppressAttr>();
  if (!Attr)
    return false;
  return llvm::any_of(Attr->diagnosticIdentifiers(), [](StringRef Identifier) {
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
