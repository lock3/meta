//===--- ScopeInfo.cpp - Information about a semantic context -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements FunctionScopeInfo and its subclasses, which contain
// information about a single function, block, lambda, or method body.
//
//===----------------------------------------------------------------------===//

#include "clang/Sema/Sema.h"
#include "clang/Analysis/AnalysisDeclContext.h"
#include "clang/Analysis/CFG.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/StmtVisitor.h"
#include "llvm/ADT/DenseMap.h" 
#include "TreeTransform.h"
#include <vector>

using namespace clang;
using namespace sema;

bool Sema::isMovableParameter(const ParmVarDecl *D) {
  QualType T = D->getType();
  if (const auto *PT = dyn_cast<ParameterType>(T)) {
    T = PT->getParameterType();
    if (PT->isInParameter())
      return InParameterType::isPassByReference(Context, T);
    if (PT->isMoveParameter())
      return MoveParameterType::isPassByReference(Context, T);
  }
  return false;
}

namespace {

// Represents the last use of a variable. This can be definite or indefinite.
struct LastUse {

  /// Constructs an indefinite last use.    
  LastUse() : Def(false), Ref() { }

  /// Constructs a definite last use.
  LastUse(const Expr *E) : Def(true), Ref(E) { }

  bool isDefinite() const { return Def; }

  const Expr *getUse() const { return Ref; }

  bool Def;
  const Expr *Ref;
};

/// A mapping of parameters to their last use.
using UseMap = llvm::SmallDenseMap<const Decl *, LastUse>;

/// The computed set of expressions constituting last uses.
using UseSet = llvm::SmallDenseSet<const Expr *>;

// Merge the uses of B into A where the uses in A are sequenced before those
// of B. A last use of B will replace the a last use in A. That is, we simply
// overwrite existing uses.
UseMap& mergeSequencedBefore(UseMap& A, const UseMap& B) {
  for (const auto &X : B)
    A[X.first] = X.second;
  return A;
}

// Merge the uses of B into A where the uses in A are sequenced after those
// of B. Last uses of B will not replace those already in A. That is, we
// only inesrt new uses, we never replace existing ones.
UseMap& mergeSequencedAfter(UseMap& A, const UseMap& B) {
  for (const auto &X : B) {
    auto Iter = A.find(X.first);
    if (Iter == A.end())
      A.insert(X);
  }
  return A;
}

// Merge the uses of B into A where A and B are the use maps of unsequenced
// operands or function arguments. If a use of P occurs in both, then its
// use is unsequenced.
UseMap& mergeUnsequenced(UseMap& A, const UseMap& B) {
  for (const auto &X : B) {
    auto Iter = A.find(X.first);
    if (Iter == A.end())
      A.insert(X);
    else
      Iter->second = LastUse();
  }
  return A;
}

/// Merge the uses in Vec using the function Merge.
template<typename MergeFn>
UseMap mergeUses(const llvm::SmallVectorImpl<UseMap> &Vec, MergeFn Merge) {
  assert(!Vec.empty());
  UseMap Uses = Vec.front();
  for (auto Iter = Vec.begin() + 1; Iter != Vec.end(); Iter++)
    Merge(Uses, *Iter);
  return Uses;
}

/// A mapping of a use to the statement in which the use occurs.
using StmtMap = llvm::SmallDenseMap<const Expr *, const Stmt *>;

/// A set of declarations.
using DeclSet = llvm::SmallDenseSet<const Decl *>;

/// Recursively compute the last use sets for in parameters of the function.
struct UseVisitor : ConstStmtVisitor<UseVisitor, UseMap> {
  Sema &SemaRef;

  UseVisitor(Sema &S) : SemaRef(S) {}

  enum EvaluationOrder {
    Unsequenced,
    LeftToRight,
    RightToLeft,
  };

  /// Compute the use map for the arguments of function call or construction.
  template<typename T>
  UseMap getArgumentUses(const T *E, EvaluationOrder Order) {
    if (E->getNumArgs() == 0)
      return UseMap();

    // Compute the last uses of each argument ...
    llvm::SmallVector<UseMap, 8> Uses;
    Uses.resize(E->getNumArgs());
    int I = 0;
    for (const Expr *A : E->arguments()) {
      Uses[I++] = Visit(A);
    }

    // ... and then combine them.
    if (Order == Unsequenced)
      return mergeUses(Uses, mergeUnsequenced);
    else if (Order == LeftToRight)
      return mergeUses(Uses, mergeSequencedBefore);
    else
      return mergeUses(Uses, mergeSequencedAfter);
  }

  UseMap VisitPredefinedExpr(const PredefinedExpr *E) {
    return UseMap();
  }

  UseMap VisitIntegerLiteral(const IntegerLiteral *E) {
    return UseMap();
  }

  UseMap VisitFloatingLiteral(const FloatingLiteral *E) {
    return UseMap();
  }

  UseMap VisitFixedPointLiteral(const FixedPointLiteral *E) {
    return UseMap();
  }

  UseMap VisitImaginaryLiteral(const ImaginaryLiteral *E) {
    return UseMap();
  }

  UseMap VisitCharacterLiteral(const CharacterLiteral *E) {
    return UseMap();
  }

  UseMap VisitStringLiteral(const StringLiteral *E) {
    return UseMap();
  }

  UseMap VisitCXXBoolLiteralExpr(const CXXBoolLiteralExpr *E) {
    return UseMap();
  }

  UseMap VisitCXXNullPtrLiteralExpr(const CXXNullPtrLiteralExpr *E) {
    return UseMap();
  }

  UseMap VisitParenExpr(const ParenExpr *E) {
    return Visit(E->getSubExpr());
  }

  UseMap VisitArraySubscriptExpr(const ArraySubscriptExpr *E) {
    UseMap LeftUses = Visit(E->getLHS());
    UseMap RightUses = Visit(E->getRHS());
    return mergeSequencedBefore(LeftUses, RightUses);
  }

  UseMap VisitDeclRefExpr(const DeclRefExpr *E) {
    // If the declaration is an in parameter, add a use map.
    if (const auto *P = dyn_cast<ParmVarDecl>(E->getDecl())) {
      if (SemaRef.isMovableParameter(P)) {
        UseMap Uses;
        Uses.try_emplace(P, E);
        return Uses;
      }
    }
    return UseMap();
  }

  static bool isAssignmentOperator(OverloadedOperatorKind K) {
    return CXXOperatorCallExpr::isAssignmentOp(K);
  }

  static bool isLogicalOperator(OverloadedOperatorKind K) {
    return K == OO_AmpAmp || K == OO_PipePipe;
  }

  static bool isShiftOperator(OverloadedOperatorKind K) {
    return K == OO_LessLess || K == OO_GreaterGreater;
  }

  static EvaluationOrder getEvaluationOrder(const CallExpr *E) {
    if (const auto *Call = dyn_cast<CXXOperatorCallExpr>(E)) {
      OverloadedOperatorKind K = Call->getOperator();
      if (isAssignmentOperator(K))
        return RightToLeft;
      else if (isLogicalOperator(K))
        return LeftToRight;
      else if (isShiftOperator(K))
        return LeftToRight;
      else if (K == OO_Subscript)
        return LeftToRight;
      else if (K == OO_ArrowStar)
        return LeftToRight;
      else if (K == OO_Comma)
        return LeftToRight;
      else
        return Unsequenced;
    }
    return Unsequenced;
  }

  UseMap VisitCallExpr(const CallExpr *E) {
    // Compute last uses for the call. Note that the evaluation of the callee
    // is sequenced before the arguments, but the evaluation of arguments are
    // unsequenced.
    UseMap Uses = Visit(E->getCallee());
    UseMap ArgUses = getArgumentUses(E, getEvaluationOrder(E));
    return mergeSequencedBefore(Uses, ArgUses);
  }

  UseMap VisitMemberExpr(const MemberExpr *E) {
    return Visit(E->getBase());
  }

  UseMap VisitBinaryOperator(const BinaryOperator *E) {
    UseMap LeftUses = Visit(E->getLHS());
    UseMap RightUses = Visit(E->getRHS());
    switch (E->getOpcode()) {
    case BO_LAnd:
    case BO_LOr:
    case BO_Comma:
      return mergeSequencedBefore(LeftUses, RightUses);
    
    case BO_Assign:
      return mergeSequencedAfter(LeftUses, RightUses);
      break;

    default:
      return mergeUnsequenced(LeftUses, RightUses);
    }
  }

  UseMap VisitCompoundAssignmentOperator(const BinaryOperator *E) {
    UseMap LeftUses = Visit(E->getLHS());
    UseMap RightUses = Visit(E->getRHS());
    return mergeSequencedAfter(LeftUses, RightUses);
  }

  UseMap VisitUnaryOperator(const UnaryOperator *E) {
    return Visit(E->getSubExpr());
  }

  UseMap VisitConditionalOperator(const ConditionalOperator *E) {
    // The operands of conditional operators are in different blocks,
    // so report no initial uses here. We'll find them later.
    return UseMap();
  }

  UseMap VisitFullExpr(const FullExpr *E) {
    return Visit(E->getSubExpr());
  }

  UseMap VisitCastExpr(const CastExpr *E) {
    return Visit(E->getSubExpr());
  }

  UseMap VisitMaterializeTemporaryExpr(const MaterializeTemporaryExpr *E) {
    return Visit(E->getSubExpr());
  }

  UseMap VisitCXXBindTemporaryExpr(const CXXBindTemporaryExpr *E) {
    return Visit(E->getSubExpr());
  }

  UseMap VisitCXXConstructExpr(const CXXConstructExpr *E) {
    return getArgumentUses(E, Unsequenced);
  }

  UseMap VisitInitListExpr(const InitListExpr *E) {
    if (E->getNumInits() == 0)
      return UseMap();

    if (E->getNumInits() == 1)
      return Visit(E->getInit(0));

    llvm::SmallVector<UseMap, 8> Uses;
    Uses.resize(E->getNumInits());
    for (std::size_t I = 0; I < E->getNumInits(); ++I)
      Uses[I] = Visit(E->getInit(I));

    // TODO: I *think* this is the right sequencing. This typically only
    // applies during list initialization, so it's not clear if just having
    // this expression is a reasonable surrogate for that condition.
    return mergeUses(Uses, mergeSequencedBefore);
  }

  UseMap VisitDesignatedInitExpr(const DesignatedInitExpr *E) {
    return Visit(E->getInit());
  }

  UseMap VisitVAArgExpr(const VAArgExpr *E) {
    return UseMap();
  }

  UseMap VisitRecoveryExpr(const RecoveryExpr *E) {
    // Don't even try to analyze these things.
    return UseMap();
  }

  UseMap VisitCXXThrowExpr(const CXXThrowExpr *E) {
    return Visit(E->getSubExpr());
  }

  UseMap VisitStmt(const Stmt *S) {
    S->dump();
    llvm_unreachable("Unknown expression");
  }

  // Statements

  UseMap VisitDeclStmt(const DeclStmt *S) {
    const DeclGroupRef Decls = S->getDeclGroup();
    for (const Decl *D : Decls) {
      if (const auto *Var = dyn_cast<VarDecl>(D)) {
        if (const Expr *E = Var->getAnyInitializer())
          return Visit(E);
      }
    }
    return UseMap();
  }

  UseMap VisitReturnStmt(const ReturnStmt *S) {
    if (const Expr *E = S->getRetValue()) 
      return Visit(E);
    return UseMap();
  }
};

/// Maintains per-block information about the last use of movable input
/// variables.
struct LastUseSearch {

  Sema &SemaRef;
  FunctionDecl *Fn;
  CFG *G;

  LastUseSearch(Sema &S, FunctionDecl *D, CFG *G)
    : SemaRef(S), Fn(D), G(G) {}

  /// Stores the top-level statements in a block.
  using StmtSeq = llvm::SmallVector<const Stmt *, 8>;
  llvm::SmallDenseMap<CFGBlock *, StmtSeq> BlockStmts;

  /// Stores the set of statements in the function.
  using StmtSet = llvm::SmallDenseSet<const Stmt *>;
  StmtSet KnownStmts;

  /// Associates each statement with a set of last-used parameters.
  using StmtParmMap = llvm::SmallDenseMap<const Stmt *, DeclSet>;
  StmtParmMap StmtParms;

  // Search the block for all top-level statements. Also remember for all
  // blocks, which statements are, in fact, statements.
  void findStatements(CFGBlock *B, llvm::SmallVectorImpl<const Stmt *> &SS) {
    llvm::SmallDenseMap<const Stmt*, bool> Map;

    // Mark subexpressions of each element in the block.
    for (auto I = B->begin(); I != B->end(); ++I) {
      CFGElement E = *I;
      if (auto SE = E.getAs<CFGStmt>()) {
        const Stmt *S = SE->getStmt();
        for (const Stmt *K : S->children())
          Map[K] = true;
      }
    }

    // Any expressions not in Map are statements.
    for (auto I = B->begin(); I != B->end(); ++I) {
      CFGElement E = *I;
      if (auto SE = E.getAs<CFGStmt>()) {
        const Stmt *S = SE->getStmt();
        if (Map.find(S) == Map.end()) {
          SS.push_back(S);
          KnownStmts.insert(S);
        }
      }
    }
  }

  /// Stores usage in formation on a per-block basis.
  using BlockUseMap = llvm::SmallDenseMap<CFGBlock *, UseMap>;
  BlockUseMap BlockUses;

  /// Stores which statements an expression whas used int.
  StmtMap UsedIn;

  // Compute the last uses of variables within a block and store those in
  // its corresponding last-use map.
  void computeLastUses(CFGBlock *B) {
    // Get the top-level statements for the block.
    llvm::SmallVectorImpl<const Stmt *>& Stmts = BlockStmts[B];
    findStatements(B, Stmts);

    UseMap& CurrentUses = BlockUses[B];
    for (const Stmt *S : Stmts) {
      // Compute the last uses of S and merge those into the block.
      UseVisitor V(SemaRef);
      UseMap StmtUses = V.Visit(S);
      mergeSequencedBefore(CurrentUses, StmtUses);

      // Associate each initial use with the statement in which it occurs.
      for (const auto& X : StmtUses) {
        LastUse Use = X.second;
        if (Use.isDefinite())
          UsedIn[Use.getUse()] = S;
      }
    }

    // llvm::outs() << "INITIAL LAST USES " << B->getBlockID() << '\n';
    // for (const auto& X : CurrentUses) {
    //   X.first->dump();
    //   if (X.second.isDefinite())
    //     X.second.getUse()->dump();
    //   else
    //     llvm::outs() << "indefinite\n";
    // }
  }

  // Vertex colors for a DFS.
  enum class Color {
    Black, White, Gray
  };

  using ColorMap = llvm::SmallVectorImpl<Color>;

  // Returns the color of a block.
  Color& getColor(ColorMap &Colors, CFGBlock *B) {
    return Colors[B->getBlockID()];
  }

  void searchSuccessiveUses(CFGBlock *B, ColorMap &Colors, UseMap &Uses) {
    for (CFGBlock *S : B->succs()) {
      if (!S)
        continue;
      if (getColor(Colors, S) == Color::White)
        findSuccessiveUses(S, Colors, Uses);
    }
  }

  void findSuccessiveUses(CFGBlock *B, ColorMap &Colors, UseMap &PotentialUses) {
    getColor(Colors, B) = Color::Gray;

    // Remove from PotentialUses any last use occurring in this succesor.
    UseMap &MyUses = BlockUses[B];
    for (auto X : MyUses)
      PotentialUses.erase(X.first);

    searchSuccessiveUses(B, Colors, PotentialUses);

    getColor(Colors, B) = Color::Black;
  }

  // Perform a DFS starting at B to determine if there are any subsequent
  // uses of each parameter. Note that we don't initially color B as the
  // starting node. That allows back edges to find this node, inherently
  // removing all uses. This ensures that uses within a loop body are never
  // last uses.
  //
  // TODO: This is a quadratic algorithm. We could probably phrase this as
  // a set of data flow equations and have the solution converge rather
  // quickly. Of course, I'd actually have to design that algorithm.
  //
  // We might be able to do this with two sets. One that holds known last
  // uses, and one that holds propagated last uses. Each block "pushes" its
  // last uses to its predecessors. When pushed, we remove from the actual
  // last uses those that overlap.
  void verifyLastUses(CFGBlock *B) {
    llvm::SmallVector<Color, 8> Colors(G->size());
    
    // Mark all nodes unvisited.
    for (CFGBlock *B : *G)
      getColor(Colors, B) = Color::White;

    // Search through successors.
    //
    // Note that we don't color this node gray so that it gets revisited
    // in the case of loops.
    UseMap &MyUses = BlockUses[B];
    searchSuccessiveUses(B, Colors, MyUses);

    // llvm::outs() << "FINAL LAST USES " << B->getBlockID() << '\n';
    // for (auto X : MyUses) {
    //   X.first->dump();
    //   if (X.second.isDefinite())
    //     X.second.getUse()->dump();
    //   else
    //     llvm::outs() << "indefinite\n";
    // }
  }

  /// This value is computed as one of the "return" values of the search.
  UseSet LastUses;

  /// Add definite last uses in B to Uses.
  void collectLastUses(CFGBlock *B) {
    UseMap &LocalUses = BlockUses[B];
    for (auto X : LocalUses) {
      if (X.second.isDefinite()) {
        const Expr *E = X.second.getUse();

        // Record the definite last use.
        LastUses.insert(E);

        // Get the statement containing the expression and add the input
        // parameter to the statements parameter set.
        const Stmt *S = UsedIn[E];
        StmtParms[S].insert(X.first);
      }
    }
  }

  void computeLastUses() {
    // Compute the initial set of last uses.
    for (auto Iter = G->begin(); Iter != G->end(); ++Iter)
      computeLastUses(*Iter);
    
    // Remove non-last uses.
    for (auto Iter = G->begin(); Iter != G->end(); ++Iter)
      verifyLastUses(*Iter);

    // Collect the set of statements comprising last uses.
    for (auto Iter = G->begin(); Iter != G->end(); ++Iter)
      collectLastUses(*Iter);
  }
};

struct LastUseFinder : RecursiveASTVisitor<LastUseFinder> {
  LastUseSearch &LU;
  DeclSet &Decls;

  LastUseFinder(LastUseSearch &LU, DeclSet &DS) : LU(LU), Decls(DS) {}

  bool VisitDeclRefExpr(DeclRefExpr *E)
  {
    if (LU.LastUses.find(E) != LU.LastUses.end())
      Decls.insert(E->getDecl());
    return true;
  }
};

/// A tree transform that rewrites the function body, substituting last uses
/// wherever they occur in the program.
struct Rewriter : TreeTransform<Rewriter> {
  using BaseType = TreeTransform<Rewriter>;

  /// Provides context for the rewriting.
  LastUseSearch &LU;

  Rewriter(Sema &SemaRef, LastUseSearch &LU)
    : BaseType(SemaRef), LU(LU) {}

  ExprResult TransformReferenceToMove(DeclRefExpr *E) {
    SourceLocation Loc = E->getExprLoc();
    ASTContext &Ctx = getSema().Context;

    auto *ParmDecl = cast<ParmVarDecl>(E->getDecl());
    auto *ParmType = cast<ParameterType>(ParmDecl->getType());

    QualType T = E->getType().getUnqualifiedType();
    Expr *R = E;

    // For in parameter, we need an initial const_cast<T&>(r).
    if (ParmType->isInParameter()) {
      QualType T1 = Ctx.getLValueReferenceType(T);
      TypeSourceInfo *TI1 = Ctx.getTrivialTypeSourceInfo(T1, Loc);
      R = getSema().BuildCXXNamedCast(Loc, tok::kw_const_cast, TI1, R,
                                      SourceRange(Loc, Loc),
                                      SourceRange(Loc, Loc)).get();
    }

    // For in and move parameters, we need a static_cast<T&&>(r)
    QualType T2 = Ctx.getRValueReferenceType(T);
    TypeSourceInfo *TI2 = Ctx.getTrivialTypeSourceInfo(T2, Loc);
    return getSema().BuildCXXNamedCast(Loc, tok::kw_static_cast, TI2,
                                       R, SourceRange(Loc, Loc),
                                       SourceRange(Loc, Loc));
  }

  /// Returns true if E is a last use.
  bool isLastUse(DeclRefExpr *E) {
    return LU.LastUses.find(E) != LU.LastUses.end();
  }

  /// Returns true if we should transform E into a move.
  bool shouldMove(DeclRefExpr *E) {
    if (isLastUse(E)) {
      const ValueDecl *D = E->getDecl();
      return CurrentParms.find(D) != CurrentParms.end();
    }
    return false;
  }

  ExprResult TransformDeclRefExpr(DeclRefExpr *E) {
    // If this is a last use of a parameter that is nominated for moving,
    // transform the reference into a sequence of casts that move the object
    // along. Otherwise, leave it as it is.
    if (shouldMove(E))
      return TransformReferenceToMove(E);
    return ExprResult(E);
  }

  // Statements

  StmtResult TransformStmt(Stmt *S, StmtDiscardKind SDK = SDK_Discarded) {
    if (!S)
      return StmtResult();
    if (isa<Expr>(S))
      return TransformExprStmt(S);
    return BaseType::TransformStmt(S, SDK);
  }

  StmtResult TransformDeclStmt(DeclStmt *S)
  {
    // FIXME: If this is a declstmt that declares a variable, we need
    // to completely refactor the declaration so that it's allocated
    // via placement new. For example:
    //
    //    foo g(in foo x) {
    //      foo z = x;
    //      return z;
    //    }
    //
    // A naive transformation gives us this:
    //
    //    foo g(in foo x) {
    //      if (<cond>)
    //        foo z = move(x);
    //      else
    //        foo z = x;
    //      return z;
    //
    // Which is clearly ill-formed. What we really want to do is this:
    //
    //    foo g(in foo x) {
    //      foo* pz;
    //      if (<cond>)
    //        new (pz) foo(move(x));
    //      else
    //        new (pz) foo(x);
    //      foo& z = *pz;
    //      return z;
    //
    // Of course, that's probably going to be pretty hard.
    //
    // In the meantime int might be sufficient to just punt on these
    // things and just generate normal code.
    //
    // Also, note that the naive implementation generates moves for both
    // initializations of the variable. This is because we aren't
    // generating new variables in each branches (and I don't think we
    // want to).
    return BaseType::TransformDeclStmt(S);
  }

  /// The current parameter for which we are rewriting an expression.
  DeclSet CurrentParms;

  StmtResult RewriteInnermostStmt(Stmt *S, bool Cleanups) {
    StmtResult R = BaseType::TransformStmt(S);
    if (Cleanups)
      R = SemaRef.MaybeCreateExprWithCleanups(cast<Expr>(R.get()));
    return R;
  }

  // Suppose we have this:
  //
  //    void f(in T x, in T y) { return x + y; }
  //
  // This needs to become:
  //
  //    void f(T const& x, T const& y) {
  //      if (x') {
  //        if (y') {
  //          return move(x) + move(y)
  //        } else {
  //          return move(x) + y;
  //        }
  //      } else {
  //        if (y') {
  //          return move(x) + move(y)
  //        } else {
  //          return move(x) + y;
  //        }
  //      }
  //    }
  //
  // This function performs a recursive construction of the transformed
  // statements.
  //
  // This is just a template to make the declaration shorter.
  template<typename I>
  StmtResult RewriteStmt(Stmt *S, I Iter, I Last, bool Cleanups) {
    // If this is the last of the parameters, then we transform the statement
    // in the "normal" way, albeit with the current set of substitutions.
    const auto *D = cast<ParmVarDecl>(*Iter);
    const auto *T = cast<ParameterType>(D->getType());

    // For in parameters, we need to build two branches, one where the current
    // parameter is moved and one where it isn't.
    if (T->isInParameter()) {
      if (std::next(Iter) == Last) {
        // Build an if/else around the original statement, moving the current
        // set of parameters. 
        CurrentParms.insert(D);
        StmtResult S0 = RewriteInnermostStmt(S, Cleanups);
        CurrentParms.erase(D);
        StmtResult S1 = RewriteInnermostStmt(S, Cleanups);
        return BuildConditionedStmt(D, S0.get(), S1.get());
      } else {
        // Build an if/else for the current parameter.
        ++Iter;
        CurrentParms.insert(D);
        StmtResult S0 = RewriteStmt(S, Iter, Last, Cleanups);
        CurrentParms.erase(D);
        StmtResult S1 = RewriteStmt(S, Iter, Last, Cleanups);
        return BuildConditionedStmt(D, S0.get(), S1.get());
      }
    }

    // Always rewrite last uses for move parameters.
    if (T->isMoveParameter()) {
      CurrentParms.insert(D);
      StmtResult R = (std::next(Iter) == Last) ?
          RewriteInnermostStmt(S, Cleanups) :
          RewriteStmt(S, Iter, Last, Cleanups);
      CurrentParms.erase(D);
      return R;
    }

    llvm_unreachable("Unhandled transformation");
  }

  StmtResult BuildConditionedStmt(const Decl *D, Stmt* S0, Stmt* S1) {
    ASTContext &Cxt = SemaRef.Context;
    SourceLocation Loc;
    ValueDecl *VD = const_cast<ValueDecl*>(cast<ValueDecl>(D));
    Expr *Cond = new (Cxt) CXXParameterInfoExpr(VD, Cxt.BoolTy, Loc);
    return IfStmt::Create(Cxt, Loc, false, nullptr, nullptr, Cond, S0, Loc, S1);
  }

  /// Finds any last uses in S, storing them in Decls. Returns true if Decls
  /// is non-empty.
  bool containsLastUses(Stmt *S, DeclSet& Decls)
  {
    LastUseFinder F(LU, Decls);
    F.TraverseStmt(S);
    return !Decls.empty();
  }

  StmtResult TransformExprStmt(Stmt *S) {
    // Recursively search the expression for last uses. If any, then we need
    // to rewrite the entire statement.
    DeclSet Decls;
    if (containsLastUses(S, Decls)) {
      // Note the presence of cleanups here. We need to add them to the
      // innermost rewritten statment.
      auto *Cleanups = dyn_cast<ExprWithCleanups>(S);
      if (Cleanups)
        S = Cleanups->getSubExpr();

      return RewriteStmt(S, Decls.begin(), Decls.end(), Cleanups);
    }

    // Otherwise, we don't need to change anything.
    return BaseType::TransformStmt(S);
  }

  // FIXME: We need to check a bunch of other statements.

  // Make sure the transform actually processes initializers.
  Decl *TransformDefinition(SourceLocation Loc, Decl *D) {
    if (auto *Var = dyn_cast<VarDecl>(D)) {
      if (Var->hasInit()) {
        // FIXME: We're not handling initializers correctly. For example,
        // if this is direct or list initialization, the flags won't be
        // set correctly, and we'll likely core dump.
        ExprResult E = TransformInitializer(Var->getInit(), false);
        getSema().AddInitializerToDecl(Var, E.get(), false);
      }
    }
    return D;
  }
};

} // namespace

void Sema::computeMoveOnLastUse(FunctionDecl *D) {
  // Don't process function templates.
  if (D->getType()->isDependentType())
    return;

  // Construct the analysis context with the default CFG build options.
  AnalysisDeclContext AC(nullptr, D);
  AC.getCFGBuildOptions().setAllAlwaysAdd();

  CFG *G = AC.getCFG();
  if (!G)
    return;
  // D->dump();
  // G->dump(Context.getLangOpts(), true);
  // llvm::outs() << "****************\n";

  LastUseSearch LU(*this, D, G);
  LU.computeLastUses();
  if (LU.LastUses.empty())
    return;

  // If there are last uses, rewrite the function body.
  Rewriter R(*this, LU);
  StmtResult S = R.TransformStmt(D->getBody());
  D->setBody(S.get());
  // D->print(llvm::outs());
}
