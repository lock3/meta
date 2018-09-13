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
class PSetsBuilder : public ConstStmtVisitor<PSetsBuilder, void> {

  LifetimeReporterBase &Reporter;
  ASTContext &ASTCtxt;
  /// Returns true if the first argument is implicitly convertible
  /// into the second argument.
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
  static const Expr *IgnoreTransparentExprs(const Expr *E,
                                            bool IgnoreLValueToRValue = false) {
    while (true) {
      E = E->IgnoreParens();
      if (const auto *P = dyn_cast<CastExpr>(E)) {
        switch (P->getCastKind()) {
        case CK_BitCast:
        case CK_LValueBitCast:
        case CK_IntegralToPointer:
        case CK_NullToPointer:
          return E;
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
      } else if (const auto *C = dyn_cast<OpaqueValueExpr>(E)) {
        E = C->getSourceExpr();
        continue;
      } else if (const auto *C = dyn_cast<UnaryOperator>(E)) {
        if (C->getOpcode() == UO_Extension) {
          E = C->getSubExpr();
          continue;
        }
      } else if (const auto *C = dyn_cast<CXXBindTemporaryExpr>(E)) {
        E = C->getSubExpr();
        continue;
      }
      return E;
    }
  }

  bool IsIgnoredStmt(const Stmt *S) {
    const Expr *E = dyn_cast<Expr>(S);
    return E && IgnoreTransparentExprs(E) != E;
  }

  void VisitStringLiteral(const StringLiteral *SL) {
    setPSet(SL, PSet::staticVar(false));
  }

  void VisitPredefinedExpr(const PredefinedExpr *E) {
    setPSet(E, PSet::staticVar(false));
  }

  void VisitDeclStmt(const DeclStmt *DS) {
    for (const auto *DeclIt : DS->decls()) {
      if (const auto *VD = dyn_cast<VarDecl>(DeclIt))
        VisitVarDecl(VD);
    }
  }

  void VisitExpr(const Expr *E) {
    assert(!hasPSet(E) || PSetsOfExpr.find(E) != PSetsOfExpr.end());
    assert(!E->isLValue() || RefersTo.find(E) != RefersTo.end());
  }

  void VisitCXXNewExpr(const CXXNewExpr *E) {
    setPSet(E, PSet::staticVar(false));
  }

  void VisitAddrLabelExpr(const AddrLabelExpr *E) {
    setPSet(E, PSet::staticVar(false));
  }

  void VisitCXXDefaultInitExpr(const CXXDefaultInitExpr *E) {
    if (hasPSet(E))
      setPSet(E, getPSet(E->getExpr()));
  }

  PSet varRefersTo(Variable V, SourceRange Range) {
    if (V.getType()->isLValueReferenceType()) {
      auto P = getPSet(V);
      if (CheckPSetValidity(P, Range))
        return P;
      else
        return PSet();
    } else {
      return PSet::singleton(V, false);
    }
  };

  void VisitDeclRefExpr(const DeclRefExpr *DeclRef) {
    if (isa<FunctionDecl>(DeclRef->getDecl()) ||
        DeclRef->refersToEnclosingVariableOrCapture()) {
      setPSet(DeclRef, PSet::staticVar(false));
    } else if (const auto *VD = dyn_cast<VarDecl>(DeclRef->getDecl())) {
      setPSet(DeclRef, varRefersTo(VD, DeclRef->getSourceRange()));
    } else if (const auto *FD = dyn_cast<FieldDecl>(DeclRef->getDecl())) {
      Variable V = Variable::thisPointer();
      V.addFieldRef(FD);
      setPSet(DeclRef, varRefersTo(V, DeclRef->getSourceRange()));
    }
  }

  void VisitMemberExpr(const MemberExpr *ME) {
    PSet BaseRefersTo = getPSet(ME->getBase());
    // Make sure that derefencing a dangling pointer is diagnosed unless
    // the member is a member function. In that case, the invalid
    // base will be diagnosed in VisitCallExpr().
    if (ME->getBase()->getType()->isPointerType() &&
        !ME->hasPlaceholderType(BuiltinType::BoundMember))
      CheckPSetValidity(BaseRefersTo, ME->getSourceRange());

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
    PSet Ref = PSet::invalid(
        InvalidationReason::PointerArithmetic(E->getSourceRange()));
    if (DeclRef) {
      const auto *VD = dyn_cast<VarDecl>(DeclRef->getDecl());
      assert(VD);
      if (VD->getType().getCanonicalType()->isArrayType())
        Ref = PSet::singleton(VD, false);
    }
    setPSet(E, Ref);
  }

  void VisitCXXThisExpr(const CXXThisExpr *E) {
    setPSet(E, PSet::singleton(Variable::thisPointer(), false));
  }

  void VisitAbstractConditionalOperator(const AbstractConditionalOperator *E) {
    // If the condition is trivially true/false, the corresponding branch
    // will be pruned from the CFG and we will not find a pset of it.
    // With AllowNonExisting, getPSet() will then return (unknown).
    // Note that a pset could also be explicitly unknown to suppress
    // further warnings after the first violation was diagnosed.
    auto LHS = getPSet(E->getTrueExpr(), /*AllowNonExisting=*/true);
    auto RHS = getPSet(E->getFalseExpr(), /*AllowNonExisting=*/true);
    setPSet(E, LHS + RHS);
  }

  void VisitMaterializeTemporaryExpr(const MaterializeTemporaryExpr *E) {
    PSet Singleton = PSet::singleton(E, false, 0);
    setPSet(E, Singleton);
    if (hasPSet(E->GetTemporaryExpr()))
      setPSet(Singleton, getPSet(E->GetTemporaryExpr()), E->getSourceRange());
  }

  void VisitInitListExpr(const InitListExpr *I) {
    if (I->isSyntacticForm())
      I = I->getSemanticForm();

    if (I->getType()->isPointerType()) {
      if (I->getNumInits() == 0) {
        setPSet(I, PSet::null(I->getSourceRange()));
        return;
      }
      if (I->getNumInits() == 1) {
        setPSet(I, getPSet(I->getInit(0)));
        return;
      }
    }
    setPSet(I, PSet::singleton(Variable::temporary()));
  }

  void VisitCastExpr(const CastExpr *E) {
    // Some casts are transparent, see IgnoreTransparentExprs()
    switch (E->getCastKind()) {
    case CK_BitCast:
    case CK_LValueBitCast:
    case CK_IntegralToPointer:
      // Those casts are forbidden by the type profile
      setPSet(E, PSet::invalid(
                     InvalidationReason::ForbiddenCast(E->getSourceRange())));
      return;
    case CK_NullToPointer:
      setPSet(E, PSet::null(E->getSourceRange()));
      return;
    case CK_LValueToRValue:
      // For L-values, the pset refers to the memory location,
      // which in turn points to the pointee. For R-values,
      // the pset just refers to the pointee.
      if (hasPSet(E))
        setPSet(E, derefPSet(getPSet(E->getSubExpr())));
      return;
    default:
      llvm_unreachable("Should have been ignored in IgnoreTransparentExprs()");
      return;
    }
  }

  void VisitBinAssign(const BinaryOperator *BO) {
    auto TC = classifyTypeCategory(BO->getType());

    if (TC == TypeCategory::Owner) {
      // Owners usually are user defined types. We should see a function call.
      // Do we need to handle raw pointers annotated as owners?
    } else if (TC == TypeCategory::Pointer) {
      // This assignment updates a Pointer.
      setPSet(getPSet(BO->getLHS()), getPSet(BO->getRHS()),
              BO->getSourceRange());
    }

    setPSet(BO, getPSet(BO->getLHS()));
  }

  void VisitBinaryOperator(const BinaryOperator *BO) {
    if (BO->getOpcode() == BO_Assign) {
      VisitBinAssign(BO);
    } else if (hasPSet(BO)) {
      setPSet(BO, PSet::invalid(InvalidationReason::PointerArithmetic(
                      BO->getSourceRange())));
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
      CheckPSetValidity(PS, UO->getSourceRange());
      setPSet(UO, PS);
      return;
    }
    default:
      if (UO->getType()->isPointerType() || UO->getType()->isArrayType()) {
        setPSet(UO, getPSet(UO->getSubExpr()));
        setPSet(getPSet(UO->getSubExpr()),
                PSet::invalid(InvalidationReason::PointerArithmetic(
                    UO->getSourceRange())),
                UO->getSourceRange());
      }
      return;
    }
  }

  void VisitReturnStmt(const ReturnStmt *R) {
    if (const Expr *RetVal = R->getRetValue()) {
      if (!isPointer(RetVal))
        return;
      auto RetPSet = getPSet(RetVal);
      if (RetPSet.containsInvalid()) {
        Reporter.warnReturnDangling(R->getReturnLoc(), false);
        RetPSet.explainWhyInvalid(Reporter);
      } else if (!RetPSet.isSubstitutableFor(PSetOfAllParams)) {
        Reporter.warnReturnWrongPset(R->getReturnLoc(), RetPSet.str(),
                                     PSetOfAllParams.str());
      }
    }
  }

  void VisitLambdaExpr(const LambdaExpr *E) {
    if (!hasPSet(E))
      return;
    PSet Set;
    for (auto Capture : E->captures()) {
      if (!Capture.capturesVariable())
        continue;
      const VarDecl *VD = Capture.getCapturedVar();
      // TODO: better location for the possible warning?
      PSet CaptureSet = varRefersTo(VD, E->getSourceRange());
      if (Capture.getCaptureKind() == LCK_ByCopy)
        CaptureSet = getPSet(CaptureSet);
      Set.merge(CaptureSet);
    }
    setPSet(E, Set);
  }

  void VisitCXXConstructExpr(const CXXConstructExpr *E) {
    if (isPointer(E)) {
      if (E->getNumArgs() == 0) {
        setPSet(E, PSet::null(E->getSourceRange()));
        return;
      }
      auto TC = classifyTypeCategory(E->getArg(0)->getType());
      if (TC == TypeCategory::Owner ||
          E->getConstructor()->isCopyOrMoveConstructor())
        setPSet(E, derefPSet(getPSet(E->getArg(0))));
      else if (TC == TypeCategory::Pointer)
        setPSet(E, getPSet(E->getArg(0)));
      else
        setPSet(E, PSet::invalid(InvalidationReason::NotInitialized(
                       E->getSourceRange())));
    } else {
      // Constructing a temporary owner/value
      setPSet(E, PSet::singleton(Variable::temporary()));
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
      if (Parents.empty())
        return;
      setPSet(E, PSet::null(Parents[0].getSourceRange()));
    }
  }

  void VisitCXXThrowExpr(const CXXThrowExpr *TE) {
    if (!isPointer(TE->getSubExpr()))
      return;
    PSet ThrownPSet = getPSet(TE->getSubExpr());
    if (!ThrownPSet.isStatic())
      Reporter.warnNonStaticThrow(TE->getLocEnd(), ThrownPSet.str());
  }

  struct CallArgument {
    CallArgument(SourceRange Range, PSet PS, QualType QType)
        : Range(Range), PS(std::move(PS)), ParamQType(QType) {}
    SourceRange Range;
    PSet PS;
    QualType ParamQType;
  };

  struct CallArguments {
    std::vector<CallArgument> Input_weak;
    std::vector<CallArgument> Oinvalidate;
    std::vector<CallArgument> Input;
    // A “function output” means a return value or a parameter passed by
    // Pointer to non-const (and is not considered to include the top-level
    // Pointer, because the output is the pointee).
    std::vector<CallArgument> Output;
  };

  void PushCallArguments(const FunctionDecl *FD, int ArgNum, SourceRange Range,
                         const Expr *Arg, QualType ParamType, bool IsInputThis,
                         CallArguments &Args) {
    // TODO implement aggregates
    if (classifyTypeCategory(ParamType) != TypeCategory::Pointer)
      return;
    QualType Pointee = getPointeeType(ParamType);
    if (Pointee.isNull())
      return;
    auto PointeeCat = classifyTypeCategory(Pointee);
    bool IsLifetimeConst = isLifetimeConst(FD, Pointee, ArgNum);

    if (ParamType->isRValueReferenceType() && PointeeCat == TypeCategory::Owner)
      return;

    if (ParamType->isLValueReferenceType() &&
        PointeeCat == TypeCategory::Owner && Pointee.isConstQualified()) {
      // all Owner arguments passed as const Owner&
      Args.Input_weak.emplace_back(Range, getPSet(Arg), ParamType);
      // the deref locations of Owners passed by const Owner&
      Args.Input_weak.emplace_back(Range, derefPSet(getPSet(Arg)), Pointee);
      return;
    }

    Args.Input.emplace_back(Range, getPSet(Arg), ParamType);
    diagnoseInput(Args.Input.back(), IsInputThis);

    // TODO: to support std::begin, we consider lifetime_const arguments as
    //       input. In the future we might have a separate annotation: gsl::in.
    if ((Pointee.isConstQualified() || IsInputThis || IsLifetimeConst ||
         ParamType->isRValueReferenceType()) &&
        (PointeeCat == TypeCategory::Owner ||
         PointeeCat == TypeCategory::Pointer)) {
      Args.Input.emplace_back(Range, derefPSet(getPSet(Arg)), Pointee);
      diagnoseInput(Args.Input.back(), IsInputThis);
    }

    if (PointeeCat == TypeCategory::Pointer && !Pointee.isConstQualified())
      Args.Output.emplace_back(Range, getPSet(Arg), Pointee);
    // Add deref this to Output for Pointer ctor?

    if (PointeeCat == TypeCategory::Owner && !IsLifetimeConst)
      Args.Oinvalidate.emplace_back(Range, getPSet(Arg), Pointee);
  }

  /// Returns the psets of each expressions in PinArgs,
  /// plus the psets of dereferencing each pset further.
  void diagnoseInput(const CallArgument &CA, bool IsInputThis) {
    if (CA.PS.containsInvalid()) {
      Reporter.warnParameterDangling(CA.Range.getBegin(),
                                     /*indirectly=*/false);
      CA.PS.explainWhyInvalid(Reporter);
    } else if (CA.PS.containsNull() &&
               (!isNullableType(CA.ParamQType) || IsInputThis)) {
      Reporter.warnParameterNull(CA.Range.getBegin(), !CA.PS.isNull());
      CA.PS.explainWhyNull(Reporter);
    }
  }

  /// Checks if the Pointer/Owner From can assign into
  /// the Pointer To.
  bool canAssign(QualType From, QualType To) {
    QualType FromPointee = getPointeeType(From);
    if (FromPointee.isNull())
      return false;

    QualType ToPointee = getPointeeType(To);
    if (ToPointee.isNull())
      return false;

    return IsConvertible(ASTCtxt.getPointerType(FromPointee),
                         ASTCtxt.getPointerType(ToPointee));
  }

  struct CallExprArguments {
    const Expr *This = nullptr;
    std::vector<const Expr *> Arguments;
  };

  CallExprArguments getArguments(const CallExpr *CallE,
                                 bool IsNonStaticMemberFunction) {
    CallExprArguments CA;
    for (unsigned I = 0; I < CallE->getNumArgs(); ++I) {
      const Expr *Arg = CallE->getArg(I);
      // For instance calls, getArg(0) is the 'this' pointer.
      if (IsNonStaticMemberFunction && isa<CXXOperatorCallExpr>(CallE) &&
          I == 0) {
        CA.This = Arg;
        continue;
      }
      CA.Arguments.push_back(Arg);
    }

    if (const auto *MemberCall = dyn_cast<CXXMemberCallExpr>(CallE))
      CA.This = MemberCall->getImplicitObjectArgument();

    return CA;
  }

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
    if (isa<CXXPseudoDestructorExpr>(CalleeE))
      return;
    CallTypes CT = getCallTypes(CalleeE);
    if (auto *OC = dyn_cast<CXXOperatorCallExpr>(CallE)) {
      assert(OC->getDirectCallee());
      if (OC->getDirectCallee()->isCXXInstanceMember())
        CT.ClassDecl = OC->getArg(0)->getType()->getAsCXXRecordDecl();

      /// Special case for assignment of Pointer into Pointer: copy pset
      if (OC->getOperator() == OO_Equal && OC->getNumArgs() == 2 &&
          classifyTypeCategory(OC->getArg(0)->getType()) ==
              TypeCategory::Pointer &&
          classifyTypeCategory(OC->getArg(1)->getType()) ==
              TypeCategory::Pointer) {
        auto PSetRHS = getPSet(getPSet(OC->getArg(1)));
        setPSet(getPSet(OC->getArg(0)), PSetRHS, CallE->getSourceRange());
        setPSet(CallE, PSetRHS);
        return;
      }
    }

    auto ParamTypes = CT.FTy->getParamTypes();
    CallExprArguments CallArgs = getArguments(CallE, CT.ClassDecl != nullptr);
    CallArguments Args;
    for (unsigned I = 0; I < CallArgs.Arguments.size(); ++I) {
      const Expr *Arg = CallArgs.Arguments[I];
      QualType ParamType = [&] {
        // For instance calls, getArg(0) is the 'this' pointer.
        if (I >= ParamTypes.size())
          return Arg->getType();
        else
          return ParamTypes[I];
      }();
      PushCallArguments(CallE->getDirectCallee(), I, Arg->getSourceRange(), Arg,
                        ParamType, /*IsInputThis=*/false, Args);
    }

    if (CT.ClassDecl) {
      // A this pointer parameter is treated as if it were declared as a
      // reference to the current object
      assert(CallArgs.This);

      QualType ObjectType = CallArgs.This->getType();
      if (ObjectType->isPointerType())
        ObjectType = ObjectType->getPointeeType();
      ObjectType = ASTCtxt.getLValueReferenceType(ObjectType);

      PushCallArguments(CallE->getDirectCallee(), -1,
                        CallArgs.This->getSourceRange(), CallArgs.This,
                        ObjectType,
                        /*IsInputThis=*/true, Args);
    }

    // TODO If p is annotated [[gsl::lifetime(x)]], then ensure that pset(p)
    // == pset(x)

    // Invalidate owners taken by Pointer to non-const.
    for (const auto &Arg : Args.Oinvalidate) {
      for (auto VarOrd : Arg.PS.vars()) {
        invalidateVar(VarOrd.first, 1, InvalidationReason::Modified(Arg.Range));
      }
    }

#if 0
    llvm::errs() << "==== Call\n";
    CallE->dump();
    CT.FTy->dump();
    for (CallArgument &CA : Args.Input) {
      llvm::errs() << "Input: " << CA.PS.str() << "\n";
      CA.ParamQType->dump();
      llvm::errs() << "\n";
    }
    for (CallArgument &CA : Args.Input_weak) {
      llvm::errs() << "Input_weak: " << CA.PS.str() << "\n";
      CA.ParamQType->dump();
      llvm::errs() << "\n";
    }
    for (CallArgument &CA : Args.Output) {
      llvm::errs() << "Output: " << CA.PS.str() << "\n";
      CA.ParamQType->dump();
      llvm::errs() << "\n";
    }
#endif

    // If p is explicitly lifetime-annotated with x, then each call site
    // enforces the precondition that argument(p) is a valid Pointer and
    // pset(argument(p)) == pset(argument(x)), and in the callee on function
    // entry set pset(p) = pset(x).

    // Enforce that pset() of each argument does not refer to a non-const
    // global Owner
    auto computeOutput = [&](QualType OutputType) {
      PSet Ret;
      for (CallArgument &CA : Args.Input) {
        if (canAssign(CA.ParamQType, OutputType))
          Ret.merge(CA.PS);
      }
      if (Ret.isUnknown()) {
        for (CallArgument &CA : Args.Input_weak) {
          if (canAssign(CA.ParamQType, OutputType))
            Ret.merge(CA.PS);
        }
      }
      if (Ret.isUnknown())
        Ret.addStatic();
      return Ret;
    };

    auto TC = classifyTypeCategory(CT.FTy->getReturnType());
    if (TC == TypeCategory::Pointer)
      setPSet(CallE, computeOutput(CT.FTy->getReturnType()));
    else
      setPSet(CallE, PSet::singleton(Variable::temporary()));

    for (const auto &Arg : Args.Output) {
      setPSet(Arg.PS, computeOutput(Arg.ParamQType), CallE->getSourceRange());
    }
  }

  bool CheckPSetValidity(const PSet &PS, SourceRange Range);

  /// Invalidates all psets that point to V or something owned by V
  void invalidateVar(Variable V, unsigned order, InvalidationReason Reason) {
    for (auto &I : PMap) {
      const auto &Pointer = I.first;
      if (Pointer == V)
        continue; // Invalidating Owner' should not change the pset of Owner
      PSet &PS = I.second;
      if (PS.containsInvalid())
        continue; // Nothing to invalidate

      if (PS.containsBase(V, order))
        setPSet(PSet::singleton(Pointer), PSet::invalid(Reason),
                Reason.getLoc());
    }
  }

  // Remove the variable from the pset together with the materialized
  // temporaries extended by that variable. It also invalidates the pointers
  // pointing to these.
  void eraseVariable(const VarDecl *VD, SourceRange Range) {
    InvalidationReason Reason =
        VD ? InvalidationReason::PointeeLeftScope(Range, VD)
           : InvalidationReason::TemporaryLeftScope(Range);
    if (VD) {
      PMap.erase(VD);
      invalidateVar(VD, 0, Reason);
    }
    // Remove all materialized temporaries that were extended by this
    // variable (or a lifetime extended temporary without an extending
    // declaration) and do the invalidation.
    for (auto I = PMap.begin(); I != PMap.end();) {
      if (I->first.isLifetimeExtendedTemporaryBy(VD)) {
        I = PMap.erase(I);
      } else {
        for (auto V : I->second.vars()) {
          if (V.first.isLifetimeExtendedTemporaryBy(VD))
            invalidateVar(V.first, 0, Reason);
        }
        ++I;
      }
    }
  }

  PSet getPSet(Variable P);

  PSet getPSet(const Expr *E, bool AllowNonExisting = false) {
    E = IgnoreTransparentExprs(E);
    if (E->isLValue()) {
      auto I = RefersTo.find(E);
      assert(I != RefersTo.end());
      return I->second;
    } else {
      auto I = PSetsOfExpr.find(E);
      assert(AllowNonExisting || I != PSetsOfExpr.end());
      if (I == PSetsOfExpr.end())
        return {};
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
  void setPSet(PSet LHS, PSet RHS, SourceRange Range);
  PSet derefPSet(const PSet &P);

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
    SourceRange Range = VD->getSourceRange();

    switch (classifyTypeCategory(VD->getType())) {
    case TypeCategory::Pointer: {
      PSet PS;
      if (VD->getType()->isArrayType()) {
        // That pset is invalid, because array to pointer decay is forbidden
        // by the bounds profile.
        // TODO: Better diagnostic that explains the array to pointer decay
        PS = PSet::invalid(InvalidationReason::PointerArithmetic(Range));
      } else if (Initializer) {
        PS = getPSet(Initializer);
      } else {
        // Never treat local statics as uninitialized.
        if (VD->hasGlobalStorage())
          PS = PSet::staticVar(false);
        else
          PS = PSet::invalid(InvalidationReason::NotInitialized(Range));
      }
      setPSet(PSet::singleton(VD), PS, Range);
      break;
    }
    case TypeCategory::Owner: {
      setPSet(PSet::singleton(VD), PSet::singleton(VD, false, 1), Range);
    }
    default:;
    }
  }

  void VisitBlock(const CFGBlock &B,
                  llvm::Optional<PSetsMap> &FalseBranchExitPMap);

  void UpdatePSetsFromCondition(const Stmt *S, bool Positive,
                                llvm::Optional<PSetsMap> &FalseBranchExitPMap,
                                SourceRange Range);
}; // namespace lifetime

// Manages lifetime information for the CFG of a FunctionDecl
PSet PSetsBuilder::getPSet(Variable P) {
  // We do not explicitly record pset(tmp) = {tmp'}.
  if (P.isTemporary())
    return PSet::singleton(P, false, 1);

  // Assumption: global Pointers have a pset of {static}
  if (P.hasStaticLifetime())
    return PSet::staticVar(false);

  auto I = PMap.find(P);
  if (I != PMap.end())
    return I->second;

  if (auto VD = P.asVarDecl()) {
    // To handle self-assignment during initialization
    if (!isa<ParmVarDecl>(VD))
      return PSet::invalid(
          InvalidationReason::NotInitialized(VD->getLocation()));
  }

  // Assume that the unseen pointer fields are valid. We will always have
  // unseen fields since we do not track the fields of owners and values.
  // Until proper aggregate support is implemented, this might be triggered
  // unintentionally.
  if (P.isField())
    return PSet::staticVar(false);

  llvm::errs() << "PSetsBuilder::getPSet: did not find pset for " << P.getName()
               << "\n";
  llvm_unreachable("Missing pset for Pointer");
}

/// Computes the pset of dereferencing a variable with the given pset
/// If PS contains (null), it is silently ignored.
PSet PSetsBuilder::derefPSet(const PSet &PS) {
  // When a local Pointer p is dereferenced using unary * or -> to create a
  // temporary tmp, then if pset(pset(p)) is nonempty, set pset(tmp) =
  // pset(pset(p)) and Kill(pset(tmp)'). Otherwise, set pset(tmp) = {tmp}.
  if (PS.isUnknown())
    return {};

  if (PS.containsInvalid())
    return {}; // Return unknown, so we don't diagnose again.

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

void PSetsBuilder::setPSet(PSet LHS, PSet RHS, SourceRange Range) {
  // Assumption: global Pointers have a pset that is a subset of {static,
  // null}
  if (LHS.isStatic() && !RHS.isUnknown() && !RHS.isStatic() && !RHS.isNull())
    Reporter.warnPsetOfGlobal(Range.getBegin(), "TODO", RHS.str());

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

bool PSetsBuilder::CheckPSetValidity(const PSet &PS, SourceRange Range) {
  if (PS.containsInvalid()) {
    Reporter.warnDerefDangling(Range.getBegin(), !PS.isInvalid());
    PS.explainWhyInvalid(Reporter);
    return false;
  }

  if (PS.containsNull()) {
    Reporter.warnDerefNull(Range.getBegin(), !PS.isNull());
    return false;
  }
  return true;
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
    SourceRange Range) {
  const auto *E = dyn_cast_or_null<Expr>(S);
  if (!E)
    return;
  E = IgnoreTransparentExprs(E, /*IgnoreLValueToRValue=*/true);
  // Handle user written bool conversion.
  if (const auto *CE = dyn_cast<CXXMemberCallExpr>(E)) {
    if (const auto *ConvDecl =
            dyn_cast_or_null<CXXConversionDecl>(CE->getDirectCallee())) {
      if (ConvDecl->getConversionType()->isBooleanType())
        UpdatePSetsFromCondition(CE->getImplicitObjectArgument(), Positive,
                                 FalseBranchExitPMap, E->getSourceRange());
    }
    return;
  }
  if (const auto *UO = dyn_cast<UnaryOperator>(E)) {
    if (UO->getOpcode() != UO_LNot)
      return;
    E = UO->getSubExpr();
    UpdatePSetsFromCondition(E, !Positive, FalseBranchExitPMap,
                             E->getSourceRange());
    return;
  }
  if (const auto *BO = dyn_cast<BinaryOperator>(E)) {
    BinaryOperator::Opcode OC = BO->getOpcode();
    if (OC != BO_NE && OC != BO_EQ)
      return;
    // The p == null is the negative case.
    if (OC == BO_EQ)
      Positive = !Positive;
    const auto *LHS = IgnoreTransparentExprs(BO->getLHS());
    const auto *RHS = IgnoreTransparentExprs(BO->getRHS());
    if (!isPointer(LHS) || !isPointer(RHS))
      return;

    if (getPSet(RHS).isNull())
      UpdatePSetsFromCondition(LHS, Positive, FalseBranchExitPMap,
                               E->getSourceRange());
    else if (getPSet(LHS).isNull())
      UpdatePSetsFromCondition(RHS, Positive, FalseBranchExitPMap,
                               E->getSourceRange());
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
      PSElseBranch = PSet::null(Range);
    } else {
      PS = PSet::null(Range);
      PSElseBranch.removeNull();
    }
    FalseBranchExitPMap = PMap;
    (*FalseBranchExitPMap)[V] = PSElseBranch;
    setPSet(PSet::singleton(V), PS, Range);
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

  auto Range = CallE->getSourceRange();
  switch (FuncNum) {
  case 1:
  case 2: {
    assert(CallE->getNumArgs() == 1 && "__lifetime_pset takes one argument");
    PSet Set = getPSet(CallE->getArg(0));

    if (FuncNum == 1) {
      if (!hasPSet(CallE->getArg(0)))
        return true; // Argument must be a Pointer or Owner
      Set = getPSet(Set);
    }
    StringRef SourceText = Lexer::getSourceText(
        CharSourceRange::getTokenRange(CallE->getArg(0)->getSourceRange()),
        ASTCtxt.getSourceManager(), ASTCtxt.getLangOpts());
    Reporter.debugPset(Range.getBegin(), SourceText, Set.str());
    return true;
  }
  case 3: {
    auto Args = Callee->getTemplateSpecializationArgs();
    auto QType = Args->get(0).getAsType();
    TypeCategory TC = classifyTypeCategory(QType);
    Reporter.debugTypeCategory(Range.getBegin(), TC);
    return true;
  }
  case 4: {
    auto QType = CallE->getArg(0)->getType();
    TypeCategory TC = classifyTypeCategory(QType);
    Reporter.debugTypeCategory(Range.getBegin(), TC);
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
      if (!IsIgnoredStmt(S))
        Visit(S);
      /*llvm::errs() << "TraverseStmt\n";
      S->dump();
      llvm::errs() << "\n";*/

      // Kill all temporaries that vanish at the end of the full expression
      if (isa<ExprWithCleanups>(S) || isa<DeclStmt>(S)) {
        invalidateVar(Variable::temporary(), 0,
                      InvalidationReason::TemporaryLeftScope(S->getLocEnd()));
        // Remove all materialized temporaries that are not extended.
        eraseVariable(nullptr, S->getLocEnd());
      }

      break;
    }
    case CFGElement::LifetimeEnds: {
      auto Leaver = E.castAs<CFGLifetimeEnds>();

      // Stop tracking Variables that leave scope.
      eraseVariable(Leaver.getVarDecl(), Leaver.getTriggerStmt()->getLocEnd());
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
} // namespace lifetime

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
    QualType ParamTy = PVD->getType();
    TypeCategory TC = classifyTypeCategory(ParamTy);
    if (TC != TypeCategory::Pointer && TC != TypeCategory::Owner)
      continue;
    QualType PointeeType = getPointeeType(ParamTy);
    Variable P(PVD);
    // Pointers initially point to their conjured deref location. But references
    // already handled as they are pointing to the deref loc.
    if (TC == TypeCategory::Pointer && !ParamTy->isReferenceType())
      P.deref();
    // Parameters cannot be invalid (checked at call site).
    PSet PS;
    // Output params are initially undefined.
    if (TC == TypeCategory::Pointer && !PointeeType.isNull() &&
        !PointeeType.isConstQualified() && !ParamTy->isRValueReferenceType()) {
      PS = PSet::invalid(
          InvalidationReason::NotInitialized(PVD->getSourceRange()));
      // It is still ok to point to output values when we return values.
      PSetForAllParams.merge(PSet::singleton(P, isNullableType(ParamTy),
                                             TC == TypeCategory::Owner));
    } else {
      PS = PSet::singleton(P, isNullableType(ParamTy),
                           TC == TypeCategory::Owner);
      PSetForAllParams.merge(PS);
    }
    PMap.emplace(PVD, std::move(PS));
  }
  PMap.emplace(Variable::thisPointer(),
               PSet::singleton(Variable::thisPointer()));
  return PSetForAllParams;
}
} // namespace lifetime
} // namespace clang