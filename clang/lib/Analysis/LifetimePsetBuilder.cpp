//=- LifetimePsetBuilder.cpp - Diagnose lifetime violations -*- C++ -*-=======//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/Analysis/Analyses/LifetimePsetBuilder.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/StmtVisitor.h"
#include "clang/Analysis/Analyses/Lifetime.h"
#include "clang/Analysis/CFG.h"
#include "clang/Lex/Lexer.h"

namespace clang {
namespace lifetime {

static bool hasPSet(const Expr *E) {
  auto TC = classifyTypeCategory(E->getType());
  return TC == TypeCategory::Pointer || TC == TypeCategory::Owner;
}

static bool isPointer(const Expr *E) {
  auto TC = classifyTypeCategory(E->getType());
  return TC == TypeCategory::Pointer;
}

/// Collection of methods to update/check PSets from statements/expressions
/// Conceptually, for each Expr where Expr::isLValue() is true,
/// we put an entry into the RefersTo map, which contains the set
/// of Variables that an lvalue might refer to, e.g.
/// RefersTo(var) = {var}
/// RefersTo(*p) = pset(p)
/// RefersTo(a = b) = {a}
/// RefersTo(a, b) = {b}
///
/// For every expression whose Type is a Pointer or an Owner,
/// we also track the pset (points-to set), e.g.
///  pset(&v) = {v}
///
// todo update RefersTo in all lvalues, namely,
// - preincrement, predecrement
// - pointer to member of object .*
// - pointer to member of pointer ->*
// - comma operator
// - string literal
// Diagnose: static_cast to lvalue ref
// TODO: handle
// - CXXDefaultArgExpr
// - CXXCtorInitializer
class PSetsBuilder : public ConstStmtVisitor<PSetsBuilder, void> {

  LifetimeReporterBase &Reporter;
  ASTContext &ASTCtxt;
  IsConvertibleTy IsConvertible;
  /// psets of all memory locations, which are identified
  /// by their non-reference variable declaration or
  /// MaterializedTemporaryExpr plus (optional) FieldDecls.
  PSetsMap &PMap;
  const PSet &PSetOfAllParams;
  std::map<const Expr *, PSet> &PSetsOfExpr;
  std::map<const Expr *, PSet> &RefersTo;

public:
  /// Ignore parentheses and most implicit casts.
  /// Does not go through implicit cast that convert a literal into a pointer,
  /// because there the type category changes.
  /// Does not ignore LValueToRValue casts by default, because they
  /// move psets from RefersTo into PsetOfExpr.
  /// Does not ignore MaterializeTemporaryExpr as Expr::IgnoreParenImpCasts
  /// would.
  static const Expr *IgnoreParenImpCasts(const Expr *E,
                                         bool IgnoreLValueToRValue = false) {
    while (true) {
      E = E->IgnoreParens();
      if (const auto *P = dyn_cast<ImplicitCastExpr>(E)) {
        switch (P->getCastKind()) {
        case CK_NullToPointer:
        case CK_LValueToRValue:
          if (!IgnoreLValueToRValue)
            return E;
          break;
        default:
          break;
        }
        E = P->getSubExpr();
        continue;
      } else if (const auto *C = dyn_cast<ExprWithCleanups>(E)) {
        E = C->getSubExpr();
        continue;
      }
      return E;
    }
  }

  void VisitStringLiteral(const StringLiteral *SL) {
    setPSet(SL, PSet::staticVar(false));
  }

  void VisitCXXBindTemporaryExpr(const CXXBindTemporaryExpr *E) {
    setPSet(E, getPSet(E->getSubExpr()));
  }

  void VisitDeclStmt(const DeclStmt *DS) {
    for (const auto *DeclIt : DS->decls()) {
      if (const auto *VD = dyn_cast<VarDecl>(DeclIt))
        VisitVarDecl(VD);
    }
  }

  void VisitImplicitCastExpr(const ImplicitCastExpr *E) {
    switch (E->getCastKind()) {
    case CK_NullToPointer:
      setPSet(E, PSet::null(E->getExprLoc()));
      return;
    case CK_LValueToRValue:
      // For L-values, the pset refers to the memory location,
      // for non-L-values we need to get the pset.
      if (hasPSet(E))
        setPSet(E, derefPSet(getPSet(E->getSubExpr()), E->getExprLoc()));
      return;
    default:
      return;
    }
  }

  void VisitExpr(const Expr *E) {
    if (IgnoreParenImpCasts(E) != E)
      return;
    assert(!hasPSet(E) || PSetsOfExpr.find(E) != PSetsOfExpr.end());
    assert(!E->isLValue() || RefersTo.find(E) != RefersTo.end());
  }

  void VisitCXXDefaultInitExpr(const CXXDefaultInitExpr *E) {
    if (hasPSet(E))
      setPSet(E, getPSet(E->getExpr()));
  }

  void VisitDeclRefExpr(const DeclRefExpr *DeclRef) {
    auto varRefersTo = [this](QualType QT, Variable V) {
      if (QT->isLValueReferenceType())
        return getPSet(V);
      else
        return PSet::singleton(V, false);
    };

    if (auto *VD = dyn_cast<VarDecl>(DeclRef->getDecl())) {
      setPSet(DeclRef, varRefersTo(VD->getType(), VD));
    } else if (auto *FD = dyn_cast<FieldDecl>(DeclRef->getDecl())) {
      Variable V = Variable::thisPointer();
      V.addFieldRef(FD);
      setPSet(DeclRef, varRefersTo(FD->getType(), V));
    }
  }

  void VisitMemberExpr(const MemberExpr *ME) {
    PSet BaseRefersTo = getPSet(ME->getBase());
    if (ME->getBase()->getType()->isPointerType())
      CheckPSetValidity(BaseRefersTo, ME->getExprLoc());

    if (auto *FD = dyn_cast<FieldDecl>(ME->getMemberDecl())) {
      PSet Ret = BaseRefersTo;
      Ret.addFieldRef(FD);
      setPSet(ME, Ret);
    } else if (isa<VarDecl>(ME->getMemberDecl())) {
      // A static data member of this class
      setPSet(ME, PSet::staticVar(false));
    }
  }

  void VisitArraySubscriptExpr(const ArraySubscriptExpr *E) {
    // By the bounds profile, ArraySubscriptExpr is only allowed on arrays
    // (not on pointers), thus the base needs to be a DeclRefExpr.
    const auto *DeclRef =
        dyn_cast<DeclRefExpr>(E->getBase()->IgnoreParenImpCasts());

    // Unless we see the actual array, we assume it is pointer arithmetic.
    PSet Ref =
        PSet::invalid(InvalidationReason::PointerArithmetic(E->getExprLoc()));
    if (DeclRef) {
      const VarDecl *VD = dyn_cast<VarDecl>(DeclRef->getDecl());
      assert(VD);
      if (VD->getType().getCanonicalType()->isArrayType())
        Ref = PSet::singleton(VD, false);
    }
    setPSet(E, Ref);
  }

  void VisitCXXThisExpr(const CXXThisExpr *E) {
    setPSet(E, PSet::singleton(Variable::thisPointer(), false));
  }

  void VisitConditionalOperator(const ConditionalOperator *E) {
    setPSet(E, getPSet(E->getLHS()) + getPSet(E->getRHS()));
  }

  void VisitMaterializeTemporaryExpr(const MaterializeTemporaryExpr *E) {
    PSet Singleton = PSet::singleton(E, false, 0);
    setPSet(E, Singleton);
    if (hasPSet(E->GetTemporaryExpr()))
      setPSet(Singleton, getPSet(E->GetTemporaryExpr()), E->getLocStart());
  }

  void VisitInitListExpr(const InitListExpr *I) {
    if (I->isSyntacticForm())
      I = I->getSemanticForm();

    if (I->getType()->isPointerType()) {
      if (I->getNumInits() == 0)
        setPSet(I, PSet::null(I->getLocStart()));
      else if (I->getNumInits() == 1)
        setPSet(I, getPSet(I->getInit(0)));
    }
  }

  void VisitExplicitCastExpr(const ExplicitCastExpr *E) {
    switch (E->getCastKind()) {
    case CK_BitCast:
    case CK_LValueBitCast:
    case CK_IntegralToPointer:
      // Those casts are forbidden by the type profile
      setPSet(
          E, PSet::invalid(InvalidationReason::ForbiddenCast(E->getExprLoc())));
      return;
    default: {
      setPSet(E, getPSet(E->getSubExpr()));
      return;
    }
    }
  }

  void VisitBinAssign(const BinaryOperator *BO) {
    auto TC = classifyTypeCategory(BO->getType());

    if (TC == TypeCategory::Owner) {
      // When an Owner x is copied to or moved to, set pset(x) = {x'}
      // setPSet(refersTo(BO->getLHS()), PSet::singleton(V, false, 1),
      // BinOp->getExprLoc());
      // TODO
    } else if (TC == TypeCategory::Pointer) {
      // This assignment updates a Pointer
      setPSet(getPSet(BO->getLHS()), getPSet(BO->getRHS()), BO->getExprLoc());
    }

    setPSet(BO, getPSet(BO->getLHS()));
  }

  void VisitBinaryOperator(const BinaryOperator *BO) {
    if (BO->getOpcode() == BO_Assign) {
      VisitBinAssign(BO);
    } else if (hasPSet(BO)) {
      setPSet(BO, PSet::invalid(
                      InvalidationReason::PointerArithmetic(BO->getExprLoc())));
    }
  }

  void VisitReturnStmt(const ReturnStmt *R) {
    if (const Expr *RetVal = R->getRetValue()) {
      if (!hasPSet(RetVal))
        return;
      if (!getPSet(RetVal).isSubstitutableFor(PSetOfAllParams)) {
        // TODO: better warning message.
        Reporter.warnDerefDangling(R->getReturnLoc(), false);
      }
    }
  }

  void VisitUnaryOperator(const UnaryOperator *UO) {
    switch (UO->getOpcode()) {
    case UO_AddrOf:
      if (hasPSet(UO))
        setPSet(UO, getPSet(UO->getSubExpr()));
      return;
    case UO_Deref: {
      auto PS = getPSet(UO->getSubExpr());
      CheckPSetValidity(PS, UO->getExprLoc());
      setPSet(UO, PS);
      return;
    }
    default:
      if (UO->getType()->isPointerType())
        setPSet(getPSet(UO->getSubExpr()),
                PSet::invalid(
                    InvalidationReason::PointerArithmetic(UO->getExprLoc())),
                UO->getExprLoc());
      return;
    }
  }

  void VisitLambdaExpr(const LambdaExpr *E) {
    // TODO: if this is a Pointer (because it captures by reference, fill the
    // pset to what it had captured)
    if (hasPSet(E))
      setPSet(E, PSet{});
  }

  void VisitCXXConstructExpr(const CXXConstructExpr *E) {
    if (isPointer(E)) {
      if (E->getNumArgs() == 0) {
        setPSet(E, PSet::null(E->getExprLoc()));
        return;
      }
      auto TC = classifyTypeCategory(E->getArg(0)->getType());
      if (TC == TypeCategory::Owner || TC == TypeCategory::Pointer)
        setPSet(E, derefPSet(getPSet(E->getArg(0)), E->getLocation()));
    }
  }

  void VisitCXXStdInitializerListExpr(const CXXStdInitializerListExpr *E) {
    if (hasPSet(E) || E->isLValue())
      setPSet(E, getPSet(E->getSubExpr()));
  }

  void VisitCXXDefaultArgExpr(const CXXDefaultArgExpr *E) {
    if (hasPSet(E) || E->isLValue())
      // FIXME: We should do setPSet(E, getPSet(E->getSubExpr())),
      // but the getSubExpr() is not visited as part of the CFG,
      // so it does not have a pset.
      setPSet(E, PSet::staticVar(false));
  }

  void VisitImplicitValueInitExpr(const ImplicitValueInitExpr *E) {
    if (E->getType()->isPointerType()) {
      // ImplicitValueInitExpr does not have a valid location
      auto Parents = ASTCtxt.getParents(*E);
      assert(!Parents.empty());
      auto *Parent = Parents[0].get<Decl>();
      assert(Parent);
      auto Loc = Parent->getLocStart();
      setPSet(E, PSet::null(Loc));
    }
  }

  struct CallArgument {
    CallArgument(SourceLocation Loc, PSet PS, QualType QType)
        : Loc(Loc), PS(std::move(PS)), ParamQType(QType) {}
    SourceLocation Loc;
    PSet PS;
    QualType ParamQType;
  };

  struct CallArguments {
    std::vector<CallArgument> Oin_weak;
    std::vector<CallArgument> Oinvalidate;
    std::vector<CallArgument> Pin;
    std::vector<CallArgument> Oin;
    // A “function output” means a return value or a parameter passed by
    // Pointer to non-const (and is not considered to include the top-level
    // Pointer, because the output is the pointee).
    std::vector<CallArgument> Pout;
  };

  void PushCallArguments(const FunctionDecl *FD, int ArgNum, SourceLocation Loc,
                         PSet Set, QualType ParamType, bool IsThisArg,
                         CallArguments &Args) {
    // TODO implement aggregates
    if (classifyTypeCategory(ParamType) == TypeCategory::Pointer) {
      if (ParamType->isRValueReferenceType())
        return;
      Args.Pin.emplace_back(Loc, Set, ParamType);
      QualType Pointee = getPointeeType(ParamType);
      auto TC = classifyTypeCategory(Pointee);
      if (!Pointee.isConstQualified()) {
        if (TC == TypeCategory::Pointer)
          Args.Pout.emplace_back(Loc, Set, Pointee);
        else if (TC == TypeCategory::Owner) {
          if (!isLifetimeConst(FD, Pointee, ArgNum))
            Args.Oinvalidate.emplace_back(Loc, Set, Pointee);
          Args.Oin.emplace_back(Loc, derefPSet(Set, Loc), Pointee);
        }
      } else if (TC == TypeCategory::Owner) {
        if (ParamType->isLValueReferenceType())
          Args.Oin_weak.emplace_back(Loc, derefPSet(Set, Loc), Pointee);
      }
      // For this, we also push the pointed value.
      if (IsThisArg) {
        if (TC == TypeCategory::Owner || TC == TypeCategory::Pointer)
          PushCallArguments(FD, 0, Loc, getPSet(Set), Pointee, false, Args);
      }
    }
  }

  /// Returns the psets of each expressions in PinArgs,
  /// plus the psets of dereferencing each pset further.
  std::vector<CallArgument>
  diagnoseAndExpandPin(const std::vector<CallArgument> &PinArgs) {
    std::vector<CallArgument> PinExtended;
    for (auto &CA : PinArgs) {
      PinExtended.emplace_back(CA.Loc, CA.PS, CA.ParamQType);

      if (CA.PS.containsInvalid()) {
        Reporter.warnParameterDangling(CA.Loc,
                                       /*indirectly=*/false);
        CA.PS.explainWhyInvalid(Reporter);
        break;
      } else if (CA.PS.containsNull() && !isNullableType(CA.ParamQType)) {
        Reporter.warnParameterNull(CA.Loc, !CA.PS.isNull());
        CA.PS.explainWhyNull(Reporter);
      }

      // TODO: expand fields.
    }
    return PinExtended;
  }

  /// Diagnose if psets arguments in Oin and Pin refer to the same variable
  /*void diagnoseParameterAliasing(const std::vector<CallArgument> &Pin,
                                 const std::vector<CallArgument> &Oin) {
    std::map<Variable, SourceLocation> AllVars;
    for (auto &CA : Pin) {
      for (auto &KV : CA.PS.vars()) {
        auto &Var = KV.first;
        // pset(argument(p)) and pset(argument(x)) must be disjoint (as long as
        // not annotated)
        auto i = AllVars.emplace(Var, CA.Loc);
        if (!i.second)
          Reporter.warnParametersAlias(CA.Loc, i.first->second, Var.getName());
      }
    }
    for (auto &CA : Oin) {
      for (auto &KV : CA.PS.vars()) {
        auto &Var = KV.first;
        // pset(argument(p)) and pset(argument(x)) must be disjoint (as long
        // as not annotated) Enforce that pset() of each argument does not
        // refer to a local Owner in Oin
        auto i = AllVars.emplace(Var, CA.Loc);
        if (!i.second)
          Reporter.warnParametersAlias(CA.Loc, i.first->second, Var.getName());
      }
    }
  }*/

  /// Evaluates the CallExpr for effects on psets.
  /// When a non-const pointer to pointer or reference to pointer is passed
  /// into a function, it's pointee's are invalidated.
  /// Returns true if CallExpr was handled.
  void VisitCallExpr(const CallExpr *CallE) {
    // Handle call to clang_analyzer_pset, which will print the pset of its
    // argument
    if (HandleClangAnalyzerPset(CallE))
      return;

    auto *CalleeE = CallE->getCallee();
    CallTypes CT = getCallTypes(CalleeE);
    auto ParamTypes = CT.FTy->getParamTypes();
    CallArguments Args;
    for (unsigned I = 0; I < CallE->getNumArgs(); ++I) {
      const Expr *Arg = CallE->getArg(I);
      bool IsThisArg = false;
      QualType ParamType = [&] {
        // For instance calls, getArg(0) is the 'this' pointer.
        if (isa<CXXOperatorCallExpr>(CallE)) {
          if (I == 0) {
            // TODO handle Arg->getType()->isPointerType()
            auto QT = ASTCtxt.getLValueReferenceType(Arg->getType());
            if (CT.FTy->isConst())
              QT.addConst();
            IsThisArg = true;
            return QT;
          } else
            return ParamTypes[I - 1];
        }
        if (I >= ParamTypes.size())
          return Arg->getType();
        else
          return ParamTypes[I];
      }();
      PushCallArguments(CallE->getDirectCallee(), I, Arg->getLocStart(),
                        getPSet(Arg), ParamType, IsThisArg, Args);
    }

    if (CT.ClassDecl) {
      // A this pointer parameter is treated as if it were declared as a
      // reference to the current object
      if (const auto *MemberCall = dyn_cast<CXXMemberCallExpr>(CallE)) {
        auto *Object = MemberCall->getImplicitObjectArgument();
        assert(Object);

        QualType ObjectType = Object->getType();
        if (ObjectType->isPointerType())
          ObjectType = ObjectType->getPointeeType();
        ObjectType = ASTCtxt.getLValueReferenceType(ObjectType);
        if (CT.FTy->isConst())
          ObjectType.addConst();

        PushCallArguments(CallE->getDirectCallee(), 0, Object->getLocStart(),
                          getPSet(Object), ObjectType, true, Args);
      }
    }

    // TODO If p is annotated [[gsl::lifetime(x)]], then ensure that pset(p)
    // == pset(x)

    std::vector<CallArgument> PinExtended = diagnoseAndExpandPin(Args.Pin);
    // diagnoseParameterAliasing(PinExtended, Args.Oin);

    // Invalidate owners taken by Pointer to non-const.
    // TODO: handle lifetime_const annotations
    for (const auto &Arg : Args.Oinvalidate) {
      for (auto VarOrd : Arg.PS.vars()) {
        invalidateVar(VarOrd.first, 0, InvalidationReason::Modified(Arg.Loc));
      }
    }

    // If p is explicitly lifetime-annotated with x, then each call site
    // enforces the precondition that argument(p) is a valid Pointer and
    // pset(argument(p)) == pset(argument(x)), and in the callee on function
    // entry set pset(p) = pset(x).

    // Enforce that pset() of each argument does not refer to a non-const
    // global Owner
    auto computeOutput = [&](QualType OutputType) {
      PSet Ret;
      QualType RetType = normalizeType(OutputType, ASTCtxt);
      for (CallArgument &CA : PinExtended) {
        if (IsConvertible(normalizeType(CA.ParamQType, ASTCtxt), RetType))
          Ret.merge(CA.PS);
      }
      for (CallArgument &CA : Args.Oin) {
        QualType CheckType = getPointerIntoOwner(CA.ParamQType, ASTCtxt);
        if (!CheckType.isNull() && IsConvertible(CheckType, RetType))
          Ret.merge(CA.PS);
      }
      if (Ret.isUnknown()) {
        for (CallArgument &CA : Args.Oin_weak) {
          QualType CheckType = getPointerIntoOwner(CA.ParamQType, ASTCtxt);
          if (!CheckType.isNull() && IsConvertible(CheckType, RetType))
            Ret.merge(CA.PS);
        }
      }
      if (Ret.isUnknown())
        Ret.addStatic();
      return Ret;
    };
    setPSet(CallE, computeOutput(CT.FTy->getReturnType()));
    for (const auto &Arg : Args.Pout) {
      setPSet(Arg.PS, computeOutput(Arg.ParamQType), CallE->getLocStart());
    }
  }

  void CheckPSetValidity(const PSet &PS, SourceLocation Loc);

  /// Invalidates all psets that point to V or something owned by V
  void invalidateVar(Variable V, unsigned order, InvalidationReason Reason) {
    for (auto &I : PMap) {
      const auto &Pointer = I.first;
      PSet &PS = I.second;
      if (PS.containsInvalid())
        continue; // Nothing to invalidate

      if (PS.containsBase(V, order))
        setPSet(PSet::singleton(Pointer), PSet::invalid(Reason),
                Reason.getLoc());
    }
  }

  void erasePointer(Variable P) { PMap.erase(P); }

  PSet getPSet(Variable P);

  PSet getPSet(const Expr *E) {
    E = IgnoreParenImpCasts(E);
    if (E->isLValue()) {
      auto I = RefersTo.find(E);
      assert(I != RefersTo.end());
      return I->second;
    } else {
      auto I = PSetsOfExpr.find(E);
      if (I == PSetsOfExpr.end())
        return PSet::singleton(Variable::temporary());
      return I->second;
    }
  }

  PSet getPSet(const PSet &P) {
    PSet Ret;
    if (P.containsInvalid())
      return PSet::invalid(P.invReasons());

    for (auto &KV : P.vars())
      Ret.merge(getPSet(KV.first));

    if (P.containsStatic())
      Ret.merge(PSet::staticVar(false));
    return Ret;
  }

  void setPSet(const Expr *E, const PSet &PS) {
    if (E->isLValue())
      RefersTo[E] = PS;
    else
      PSetsOfExpr[E] = PS;
  }
  void setPSet(PSet LHS, PSet RHS, SourceLocation Loc);
  PSet derefPSet(PSet P, SourceLocation Loc);

  bool HandleClangAnalyzerPset(const CallExpr *CallE);

public:
  PSetsBuilder(LifetimeReporterBase &Reporter, ASTContext &ASTCtxt,
               PSetsMap &PMap, const PSet &PSetOfAllParams,
               std::map<const Expr *, PSet> &PSetsOfExpr,
               std::map<const Expr *, PSet> &RefersTo,
               IsConvertibleTy IsConvertible)
      : Reporter(Reporter), ASTCtxt(ASTCtxt), IsConvertible(IsConvertible),
        PMap(PMap), PSetOfAllParams(PSetOfAllParams), PSetsOfExpr(PSetsOfExpr),
        RefersTo(RefersTo) {}

  void VisitVarDecl(const VarDecl *VD) {
    const Expr *Initializer = VD->getInit();
    SourceLocation Loc = VD->getLocEnd();

    switch (classifyTypeCategory(VD->getType())) {
    case TypeCategory::Pointer: {
      PSet PS;
      if (VD->getType()->isArrayType()) {
        // That pset is invalid, because array to pointer decay is forbidden
        // by the bounds profile.
        // TODO: Better diagnostic that explains the array to pointer decay
        PS = PSet::invalid(InvalidationReason::PointerArithmetic(Loc));
      } else if (Initializer) {
        PS = getPSet(Initializer);
      } else {
        // Never treat local statics as uninitialized.
        if (VD->hasGlobalStorage())
          PS = PSet::staticVar(false);
        else
          PS = PSet::invalid(InvalidationReason::NotInitialized(Loc));
      }
      setPSet(PSet::singleton(VD), PS, Loc);
      break;
    }
    case TypeCategory::Owner: {
      setPSet(PSet::singleton(VD), PSet::singleton(VD, false, 1), Loc);
    }
    default:;
    }
  }

  void VisitBlock(const CFGBlock &B,
                  llvm::Optional<PSetsMap> &FalseBranchExitPMap);

  void UpdatePSetsFromCondition(const Stmt *S, bool Positive,
                                llvm::Optional<PSetsMap> &FalseBranchExitPMap,
                                SourceLocation Loc);
}; // namespace lifetime

// Manages lifetime information for the CFG of a FunctionDecl
PSet PSetsBuilder::getPSet(Variable P) {
  auto I = PMap.find(P);
  if (I != PMap.end())
    return I->second;

  // Assumption: global Pointers have a pset of {static}
  if (P.hasGlobalStorage() || P.isMemberVariableOfEnclosingClass())
    return PSet::staticVar(false);

  if (auto VD = P.asVarDecl()) {
    // To handle self-assignment during initialization
    if (!isa<ParmVarDecl>(VD))
      return PSet::invalid(
          InvalidationReason::NotInitialized(VD->getLocation()));
  }

  llvm::errs() << "PSetsBuilder::getPSet: did not find pset for " << P.getName()
               << "\n";
  llvm_unreachable("Missing pset for Pointer");
}

/// Computes the pset of dereferencing a variable with the given pset
/// If PS contains (null), it is silently ignored.
PSet PSetsBuilder::derefPSet(PSet PS, SourceLocation Loc) {
  // When a local Pointer p is dereferenced using unary * or -> to create a
  // temporary tmp, then if pset(pset(p)) is nonempty, set pset(tmp) =
  // pset(pset(p)) and Kill(pset(tmp)'). Otherwise, set pset(tmp) = {tmp}.
  if (PS.isUnknown())
    return {};

  if (PS.containsInvalid()) {
    std::vector<InvalidationReason> invReasons = PS.invReasons();
    invReasons.emplace_back(InvalidationReason::Dereferenced(Loc));
    return PSet::invalid(PS.invReasons());
  }

  PSet RetPS;
  if (PS.containsStatic())
    RetPS.addStatic();

  for (auto &KV : PS.vars()) {
    const Variable &V = KV.first;
    auto order = KV.second;

    if (order > 0)
      RetPS.insert(V, order + 1); // pset(o') = { o'' }
    else
      RetPS.merge(getPSet(V));
  }

  return RetPS;
}

void PSetsBuilder::setPSet(PSet LHS, PSet RHS, SourceLocation Loc) {
  // Assumption: global Pointers have a pset that is a subset of {static,
  // null}
  if (LHS.isStatic() && !RHS.isUnknown() && !RHS.isStatic() && !RHS.isNull())
    Reporter.warnPsetOfGlobal(Loc, "TODO", RHS.str());

  if (LHS.isSingleton()) {
    Variable Var = LHS.vars().begin()->first;
    auto I = PMap.find(Var);
    if (I != PMap.end())
      I->second = std::move(RHS);
    else
      PMap.emplace(Var, RHS);
  } else {
    for (auto &KV : LHS.vars()) {
      auto I = PMap.find(KV.first);
      if (I != PMap.end())
        I->second.merge(RHS);
      else
        PMap.emplace(KV.first, RHS);
    }
  }
}

void PSetsBuilder::CheckPSetValidity(const PSet &PS, SourceLocation Loc) {
  assert(!PS.isUnknown());

  if (PS.containsInvalid()) {
    Reporter.warnDerefDangling(Loc, !PS.isInvalid());
    PS.explainWhyInvalid(Reporter);
    return;
  }

  if (PS.containsNull()) {
    Reporter.warnDerefNull(Loc, !PS.isNull());
    return;
  }
}

/// Updates psets to remove 'null' when entering conditional statements. If
/// 'positive' is false, handles expression as-if it was negated.
/// Examples:
///   int* p = f();
/// if(p)
///  ... // pset of p does not contain 'null'
/// else
///  ... // pset of p is 'null'
/// if(!p)
///  ... // pset of p is 'null'
/// else
///  ... // pset of p does not contain 'null'
void PSetsBuilder::UpdatePSetsFromCondition(
    const Stmt *S, bool Positive, llvm::Optional<PSetsMap> &FalseBranchExitPMap,
    SourceLocation Loc) {
  const auto *E = dyn_cast_or_null<Expr>(S);
  if (!E)
    return;
  E = IgnoreParenImpCasts(E, /*IgnoreLValueToRValue=*/true);
  // Handle user written bool conversion.
  if (const auto *CE = dyn_cast<CXXMemberCallExpr>(E)) {
    if (const auto *ConvDecl =
            dyn_cast_or_null<CXXConversionDecl>(CE->getDirectCallee())) {
      if (ConvDecl->getConversionType()->isBooleanType())
        UpdatePSetsFromCondition(CE->getImplicitObjectArgument(), Positive,
                                 FalseBranchExitPMap, E->getLocStart());
    }
    return;
  }
  if (const auto *UO = dyn_cast<UnaryOperator>(E)) {
    if (UO->getOpcode() != UO_LNot)
      return;
    E = UO->getSubExpr();
    UpdatePSetsFromCondition(E, !Positive, FalseBranchExitPMap,
                             E->getLocStart());
    return;
  }
  if (const auto *BO = dyn_cast<BinaryOperator>(E)) {
    BinaryOperator::Opcode OC = BO->getOpcode();
    if (OC != BO_NE && OC != BO_EQ)
      return;
    // The p == null is the negative case.
    if (OC == BO_EQ)
      Positive = !Positive;
    const auto *LHS = IgnoreParenImpCasts(BO->getLHS());
    const auto *RHS = IgnoreParenImpCasts(BO->getRHS());
    if (!isPointer(LHS) || !isPointer(RHS))
      return;

    if (getPSet(RHS).isNull())
      UpdatePSetsFromCondition(LHS, Positive, FalseBranchExitPMap,
                               E->getLocStart());
    else if (getPSet(LHS).isNull())
      UpdatePSetsFromCondition(RHS, Positive, FalseBranchExitPMap,
                               E->getLocStart());
    return;
  }

  if (E->isLValue() && hasPSet(E)) {
    auto Ref = getPSet(E);
    // We refer to multiple variables (or none),
    // and we cannot know which of them is null/non-null.
    if (Ref.vars().size() != 1)
      return;

    Variable V = Ref.vars().begin()->first;
    PSet PS = getPSet(V);
    PSet PSElseBranch = PS;
    if (Positive) {
      PS.removeNull();
      PSElseBranch = PSet::null(Loc);
    } else {
      PS = PSet::null(Loc);
      PSElseBranch.removeNull();
    }
    FalseBranchExitPMap = PMap;
    (*FalseBranchExitPMap)[V] = PSElseBranch;
    setPSet(PSet::singleton(V), PS, Loc);
  }
} // namespace lifetime

/// Checks if the statement S is a call to clang_analyzer_pset and, if yes,
/// diags the pset of its argument
bool PSetsBuilder::HandleClangAnalyzerPset(const CallExpr *CallE) {

  const FunctionDecl *Callee = CallE->getDirectCallee();
  if (!Callee)
    return false;

  const auto *I = Callee->getIdentifier();
  if (!I)
    return false;

  auto FuncNum = llvm::StringSwitch<int>(I->getName())
                     .Case("__lifetime_pset", 1)
                     .Case("__lifetime_pset_ref", 2)
                     .Case("__lifetime_type_category", 3)
                     .Case("__lifetime_type_category_arg", 4)
                     .Default(0);
  if (FuncNum == 0)
    return false;

  auto Loc = CallE->getLocStart();
  switch (FuncNum) {
  case 1:
  case 2: {
    assert(CallE->getNumArgs() == 1 && "__lifetime_pset takes one argument");
    PSet Set = getPSet(CallE->getArg(0));
    if (FuncNum == 1)
      Set = getPSet(Set);
    StringRef SourceText = Lexer::getSourceText(
        CharSourceRange::getTokenRange(CallE->getArg(0)->getSourceRange()),
        ASTCtxt.getSourceManager(), ASTCtxt.getLangOpts());
    Reporter.debugPset(Loc, SourceText, Set.str());
    return true;
  }
  case 3: {
    auto Args = Callee->getTemplateSpecializationArgs();
    auto QType = Args->get(0).getAsType();
    TypeCategory TC = classifyTypeCategory(QType);
    Reporter.debugTypeCategory(Loc, TC);
    return true;
  }
  case 4: {
    auto QType = CallE->getArg(0)->getType();
    TypeCategory TC = classifyTypeCategory(QType);
    Reporter.debugTypeCategory(Loc, TC);
    return true;
  }
  default:
    llvm_unreachable("Unknown debug function.");
  }
}

static const Stmt *getRealTerminator(const CFGBlock &B) {
  if (B.succ_size() == 1)
    return nullptr;
  const Stmt *LastCFGStmt = nullptr;
  for (const CFGElement &Element : B) {
    if (auto CFGSt = Element.getAs<CFGStmt>()) {
      LastCFGStmt = CFGSt->getStmt();
    }
  }
  return LastCFGStmt;
}

// Update PSets in Builder through all CFGElements of this block
void PSetsBuilder::VisitBlock(const CFGBlock &B,
                              llvm::Optional<PSetsMap> &FalseBranchExitPMap) {
  for (const auto &E : B) {
    switch (E.getKind()) {
    case CFGElement::Statement: {
      const Stmt *S = E.castAs<CFGStmt>().getStmt();
      Visit(S);
      /*llvm::errs() << "TraverseStmt\n";
      S->dump();
      llvm::errs() << "\n";*/

      // Kill all temporaries that vanish at the end of the full expression
      if (isa<ExprWithCleanups>(S) || isa<DeclStmt>(S))
        invalidateVar(Variable::temporary(), 0,
                      InvalidationReason::TemporaryLeftScope(S->getLocEnd()));

      break;
    }
    case CFGElement::LifetimeEnds: {
      auto Leaver = E.castAs<CFGLifetimeEnds>();

      // Stop tracking Pointers that leave scope
      erasePointer(Leaver.getVarDecl());

      // Invalidate all pointers that track leaving Owners
      invalidateVar(
          Leaver.getVarDecl(), 0,
          InvalidationReason::PointeeLeftScope(
              Leaver.getTriggerStmt()->getLocEnd(), Leaver.getVarDecl()));
      break;
    }
    case CFGElement::NewAllocator:
    case CFGElement::AutomaticObjectDtor:
    case CFGElement::DeleteDtor:
    case CFGElement::BaseDtor:
    case CFGElement::MemberDtor:
    case CFGElement::TemporaryDtor:
    case CFGElement::Initializer:
    case CFGElement::ScopeBegin:
    case CFGElement::ScopeEnd:
    case CFGElement::LoopExit:
    case CFGElement::Constructor: // TODO
    case CFGElement::CXXRecordTypedCall:
      break;
    }
  }
  if (auto *Terminator = getRealTerminator(B)) {
    UpdatePSetsFromCondition(Terminator, /*Positive=*/true, FalseBranchExitPMap,
                             Terminator->getLocEnd());
  }
}

void VisitBlock(PSetsMap &PMap, llvm::Optional<PSetsMap> &FalseBranchExitPMap,
                const PSet &PSetOfAllParams,
                std::map<const Expr *, PSet> &PSetsOfExpr,
                std::map<const Expr *, PSet> &RefersTo, const CFGBlock &B,
                LifetimeReporterBase &Reporter, ASTContext &ASTCtxt,
                IsConvertibleTy IsConvertible) {
  PSetsBuilder Builder(Reporter, ASTCtxt, PMap, PSetOfAllParams, PSetsOfExpr,
                       RefersTo, IsConvertible);
  Builder.VisitBlock(B, FalseBranchExitPMap);
}

PSet PopulatePSetForParams(PSetsMap &PMap, const FunctionDecl *FD) {
  PSet PSetForAllParams;
  for (const ParmVarDecl *PVD : FD->parameters()) {
    TypeCategory TC = classifyTypeCategory(PVD->getType());
    if (TC != TypeCategory::Pointer && TC != TypeCategory::Owner)
      continue;
    Variable P(PVD);
    // Parameters cannot be invalid (checked at call site).
    auto PS = PSet::singleton(P, P.mightBeNull(), TC == TypeCategory::Owner);
    PSetForAllParams.merge(PS);
    PMap.emplace(P, std::move(PS));
  }
  PMap.emplace(Variable::thisPointer(),
               PSet::singleton(Variable::thisPointer()));
  return PSetForAllParams;
}
} // namespace lifetime
} // namespace clang