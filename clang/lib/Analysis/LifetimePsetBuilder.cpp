//=- LifetimePsetBuilder.cpp - Diagnose lifetime violations -*- C++ -*-=======//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/Analysis/Analyses/LifetimePsetBuilder.h"
#include "clang/AST/Attr.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/StmtVisitor.h"
#include "clang/Analysis/Analyses/Lifetime.h"
#include "clang/Analysis/CFG.h"
#include "clang/Lex/Lexer.h"

namespace clang {
namespace lifetime {

namespace {

#define VERBOSE_DEBUG 0
#if VERBOSE_DEBUG
#define DBG(x) llvm::errs() << x
#else
#define DBG(x)
#endif

static bool hasPSet(const Expr *E) {
  auto TC = classifyTypeCategory(E->getType());
  return E->isLValue() || TC == TypeCategory::Pointer ||
         TC == TypeCategory::Owner;
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

  const FunctionDecl *AnalyzedFD;
  LifetimeReporterBase &Reporter;
  ASTContext &ASTCtxt;
  /// Returns true if the first argument is implicitly convertible
  /// into the second argument.
  IsConvertibleTy IsConvertible;

  /// psets of all memory locations, which are identified
  /// by their non-reference variable declaration or
  /// MaterializedTemporaryExpr plus (optional) FieldDecls.
  PSetsMap &PMap;
  std::map<const Expr *, PSet> &PSetsOfExpr;
  std::map<const Expr *, PSet> &RefersTo;
  const CFGBlock *CurrentBlock = nullptr;

  /// Certain constructs that violate the type and bounds profile of the
  /// C++ core guidelines (such as reinterpret_cast) disable lifetime analysis.
  /// We disable it for the whole function because those constructs make it
  /// hard to asses which Pointers are tainted by them.
  bool AnalysisDisabled = false;

public:
  bool isAnalysisDisabled() const { return AnalysisDisabled; }

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
        case CK_ArrayToPointerDecay:
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

  static bool IsIgnoredStmt(const Stmt *S) {
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

  PSet varRefersTo(Variable V, SourceRange Range) const {
    if (V.getType()->isReferenceType()) {
      auto P = getPSet(V);
      if (CheckPSetValidity(P, Range))
        return P;
      else
        return {};
    } else {
      return PSet::singleton(V);
    }
  };

  void VisitDeclRefExpr(const DeclRefExpr *DeclRef) {
    if (isa<FunctionDecl>(DeclRef->getDecl()) ||
        DeclRef->refersToEnclosingVariableOrCapture()) {
      setPSet(DeclRef, PSet::staticVar(false));
    } else if (const auto *VD = dyn_cast<VarDecl>(DeclRef->getDecl())) {
      setPSet(DeclRef, varRefersTo(VD, DeclRef->getSourceRange()));
    } else if (const auto *B = dyn_cast<BindingDecl>(DeclRef->getDecl())) {
      // TODO: support BindingDecl in Variables?
      setPSet(DeclRef, {});
    } else if (const auto *FD = dyn_cast<FieldDecl>(DeclRef->getDecl())) {
      Variable V = Variable::thisPointer(FD->getParent());
      V.deref();         // *this
      V.addFieldRef(FD); // this->field
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
      if (FD->getType()->isReferenceType()) {
        // The field has reference type, apply the deref.
        Ret = derefPSet(Ret);
        if (!CheckPSetValidity(Ret, ME->getSourceRange()))
          Ret = {};
      }
      setPSet(ME, Ret);
    } else if (isa<VarDecl>(ME->getMemberDecl())) {
      // A static data member of this class
      setPSet(ME, PSet::staticVar(false));
    }
  }

  void VisitArraySubscriptExpr(const ArraySubscriptExpr *E) {
    // By the bounds profile, ArraySubscriptExpr is only allowed on arrays
    // (not on pointers).

    if (!E->getBase()->IgnoreParenImpCasts()->getType()->isArrayType()) {
      Reporter.warnPointerArithmetic(E->getSourceRange());
      AnalysisDisabled = true;
      setPSet(E, {});
      return;
    }

    setPSet(E, getPSet(E->getBase()));
  }

  void VisitCXXThisExpr(const CXXThisExpr *E) {
    // ThisExpr is an RValue. It points to *this.
    setPSet(E, PSet::singleton(Variable::thisPointer(
                                   E->getType()->getPointeeCXXRecordDecl())
                                   .deref()));
  }

  void VisitCXXTypeidExpr(const CXXTypeidExpr *E) {
    // The typeid expression is an lvalue expression which refers to an object
    // with static storage duration, of the polymorphic type const
    // std::type_info or of some type derived from it.
    setPSet(E, PSet::staticVar());
  }

  void
  VisitSubstNonTypeTemplateParmExpr(const SubstNonTypeTemplateParmExpr *E) {
    // Non-type template parameters that are pointers must point to something
    // static (because only addresses known at compiler time are allowed)
    if (hasPSet(E))
      setPSet(E, PSet::staticVar());
  }

  void VisitAbstractConditionalOperator(const AbstractConditionalOperator *E) {
    if (!hasPSet(E))
      return;
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
    PSet Singleton = PSet::singleton(E);
    setPSet(E, Singleton);
    if (hasPSet(E->getSubExpr())) {
      auto TC = classifyTypeCategory(E->getSubExpr()->getType());
      if (TC == TypeCategory::Owner)
        setPSet(Singleton, PSet::singleton(E, 1), E->getSourceRange());
      else
        setPSet(Singleton, getPSet(E->getSubExpr()), E->getSourceRange());
    }
  }

  void VisitInitListExpr(const InitListExpr *I) {
    I = !I->isSemanticForm() ? I->getSemanticForm() : I;

    if (I->getType()->isPointerType()) {
      if (I->getNumInits() == 0) {
        setPSet(I, PSet::null(NullReason::defaultConstructed(
                       I->getSourceRange(), CurrentBlock)));
        return;
      }
      // TODO: Instead of assuming that the pset comes from the first argument
      // use the same logic we have in call modelling.
      if (I->getNumInits() == 1) {
        setPSet(I, getPSet(I->getInit(0)));
        return;
      }
    }
    setPSet(I, {});
  }

  void VisitCXXScalarValueInitExpr(const CXXScalarValueInitExpr *E) {
    // Appears for `T()` in a template specialisation, where
    // T is a simple type.
    if (E->getType()->isPointerType()) {
      setPSet(E, PSet::null(NullReason::defaultConstructed(E->getSourceRange(),
                                                           CurrentBlock)));
    }
  }

  void VisitCastExpr(const CastExpr *E) {
    // Some casts are transparent, see IgnoreTransparentExprs()
    switch (E->getCastKind()) {
    case CK_BitCast:
    case CK_LValueBitCast:
    case CK_IntegralToPointer:
      // Those casts are forbidden by the type profile
      Reporter.warnUnsafeCast(E->getSourceRange());
      AnalysisDisabled = true;
      setPSet(E, {});
      return;
    case CK_ArrayToPointerDecay:
      // Decaying an array into a pointer is like taking the address of the
      // first array member. The result is a pointer to the array elements,
      // which are '*array'.
      setPSet(E, derefPSet(getPSet(E->getSubExpr())));
      return;
    case CK_NullToPointer:
      setPSet(E, PSet::null(NullReason::nullptrConstant(E->getSourceRange(),
                                                        CurrentBlock)));
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

  /// Returns true if all of the following is true
  /// 1) BO is an addition,
  /// 2) LHS is ImplicitCastExpr <ArrayToPointerDecay> of DeclRefExpr of array
  /// type 3) RHS is IntegerLiteral 4) The value of the IntegerLiteral is
  /// between 0 and the size of the array type
  static bool isArrayPlusIndex(const BinaryOperator *BO) {
    if (BO->getOpcode() != BO_Add)
      return false;

    auto *ImplCast = dyn_cast<ImplicitCastExpr>(BO->getLHS());
    if (!ImplCast)
      return false;

    if (ImplCast->getCastKind() != CK_ArrayToPointerDecay)
      return false;
    auto *DeclRef = dyn_cast<DeclRefExpr>(ImplCast->getSubExpr());
    if (!DeclRef)
      return false;
    auto *ArrayType = dyn_cast_or_null<ConstantArrayType>(
        DeclRef->getType()->getAsArrayTypeUnsafe());
    if (!ArrayType)
      return false;
    llvm::APInt ArrayBound = ArrayType->getSize();

    auto *Integer = dyn_cast<IntegerLiteral>(BO->getRHS());
    if (!Integer)
      return false;

    llvm::APInt Offset = Integer->getValue();
    if (Offset.isNegative())
      return false;

    // ule() comparison requires both APInt to have the same bit width.
    if (ArrayBound.getBitWidth() > Offset.getBitWidth())
      Offset = Offset.zext(ArrayBound.getBitWidth());
    else if (ArrayBound.getBitWidth() < Offset.getBitWidth())
      ArrayBound = ArrayBound.zext(Offset.getBitWidth());

    // We explicitly allow to form the pointer one element after the array
    // to support the compiler-generated code for the end iterator in a
    // for-range loop.
    // TODO: this allows to form an invalid pointer, where we would not detect
    // dereferencing.
    return Offset.ule(ArrayBound);
  }

  void VisitBinaryOperator(const BinaryOperator *BO) {
    if (BO->getOpcode() == BO_Assign) {
      // Owners usually are user defined types. We should see a function call.
      // Do we need to handle raw pointers annotated as owners?
      if (isPointer(BO)) {
        // This assignment updates a Pointer.
        SourceRange Range = BO->getRHS()->getSourceRange();
        PSet LHS = handlePointerAssign(BO->getLHS()->getType(),
                                       getPSet(BO->getRHS()), Range);
        setPSet(getPSet(BO->getLHS()), LHS, Range);
      }

      setPSet(BO, getPSet(BO->getLHS()));
    } else if (isArrayPlusIndex(BO)) {
      setPSet(BO, getPSet(BO->getLHS()));
    } else if (BO->getType()->isPointerType()) {
      Reporter.warnPointerArithmetic(BO->getOperatorLoc());
      AnalysisDisabled = true;
      setPSet(BO, {});
    } else if (BO->isLValue() && BO->isCompoundAssignmentOp()) {
      setPSet(BO, getPSet(BO->getLHS()));
    } else if (BO->isLValue() && BO->getOpcode() == BO_Comma) {
      setPSet(BO, getPSet(BO->getRHS()));
    } else if (BO->getOpcode() == BO_PtrMemD || BO->getOpcode() == BO_PtrMemI) {
      setPSet(BO, {}); // TODO, not specified in paper
    }
  }

  void VisitUnaryOperator(const UnaryOperator *UO) {
    switch (UO->getOpcode()) {
    case UO_AddrOf:
      break;
    case UO_Deref: {
      auto PS = getPSet(UO->getSubExpr());
      CheckPSetValidity(PS, UO->getSourceRange());
      setPSet(UO, PS);
      return;
    }
    default:
      // Workaround: detecting compiler generated AST node.
      if (isPointer(UO) && UO->getBeginLoc() != UO->getEndLoc()) {
        Reporter.warnPointerArithmetic(UO->getOperatorLoc());
        AnalysisDisabled = true;
        setPSet(getPSet(UO->getSubExpr()), {}, UO->getSourceRange());
      }
    }

    if (hasPSet(UO))
      setPSet(UO, getPSet(UO->getSubExpr()));
  }

  void VisitReturnStmt(const ReturnStmt *R) {
    const Expr *RetVal = R->getRetValue();
    if (!RetVal)
      return;

    if (!isPointer(RetVal) && !RetVal->isLValue())
      return;

    auto RetPSet = getPSet(RetVal);

    // TODO: Would be nicer if the LifetimeEnds CFG nodes would appear before
    // the ReturnStmt node
    for (auto &Var : RetPSet.vars()) {
      if (Var.isTemporary()) {
        RetPSet = PSet::invalid(InvalidationReason::TemporaryLeftScope(
            R->getSourceRange(), CurrentBlock));
        break;
      } else if (auto *VD = Var.asVarDecl()) {
        // Allow to return a pointer to *p (then p is a parameter).
        if (VD->hasLocalStorage() &&
            (!Var.isDeref() ||
             classifyTypeCategory(VD->getType()) == TypeCategory::Owner)) {
          RetPSet = PSet::invalid(InvalidationReason::PointeeLeftScope(
              R->getSourceRange(), CurrentBlock, VD));
          break;
        }
      }
    }
    PSetsMap PostConditions;
    getLifetimeContracts(PostConditions, AnalyzedFD, ASTCtxt, CurrentBlock,
                         IsConvertible, Reporter, /*Pre=*/false);
    RetPSet.checkSubstitutableFor(PostConditions[Variable::returnVal()],
                                  R->getSourceRange(), Reporter,
                                  ValueSource::Return);
  }

  void VisitCXXConstructExpr(const CXXConstructExpr *E) {
    if (!isPointer(E)) {
      // Constructing a temporary owner/value
      setPSet(E, {});
      return;
    }

    if (E->getNumArgs() == 0) {
      setPSet(E, PSet::null(NullReason::defaultConstructed(E->getSourceRange(),
                                                           CurrentBlock)));
      return;
    }

    auto Ctor = E->getConstructor();
    auto ParmTy = Ctor->getParamDecl(0)->getType();
    auto TC = classifyTypeCategory(E->getArg(0)->getType());
    // For ctors taking a const reference we assume that we will not take the
    // address of the argument but copy it.
    // TODO: Use the function call rules here.
    if (TC == TypeCategory::Owner || Ctor->isCopyOrMoveConstructor() ||
        (ParmTy->isReferenceType() &&
         ParmTy->getPointeeType().isConstQualified()))
      setPSet(E, derefPSet(getPSet(E->getArg(0))));
    else if (TC == TypeCategory::Pointer)
      setPSet(E, getPSet(E->getArg(0)));
    else
      setPSet(E, PSet::invalid(InvalidationReason::NotInitialized(
                     E->getSourceRange(), CurrentBlock)));
  }

  void VisitCXXStdInitializerListExpr(const CXXStdInitializerListExpr *E) {
    if (hasPSet(E))
      setPSet(E, getPSet(E->getSubExpr()));
  }

  void VisitCXXDefaultArgExpr(const CXXDefaultArgExpr *E) {
    if (hasPSet(E))
      // FIXME: We should do setPSet(E, getPSet(E->getSubExpr())),
      // but the getSubExpr() is not visited as part of the CFG,
      // so it does not have a pset.
      setPSet(E, PSet::staticVar(false));
  }

  void VisitImplicitValueInitExpr(const ImplicitValueInitExpr *E) {
    // We don't really care, because this expression is not referenced
    // anywhere. But still set it to satisfy the VisitStmt() post-condition.
    setPSet(E, {});
  }

  void VisitCompoundLiteralExpr(const CompoundLiteralExpr *E) {
    // C99 construct. We ignore it, but still set the pset to satisfy the
    // VisitStmt() post-condition.
    setPSet(E, {});
  }

  void VisitVAArgExpr(const VAArgExpr *E) { setPSet(E, {}); }

  void VisitStmtExpr(const StmtExpr *E) {
    // TODO: not yet suppported.
    setPSet(E, {});
  }

  void VisitCXXDeleteExpr(const CXXDeleteExpr *DE) {
    if (hasPSet(DE->getArgument())) {
      PSet PS = getPSet(DE->getArgument());
      for (const auto &Var : PS.vars()) {
        // TODO: diagnose if we are deleting the buffer of on owner?
        invalidateVar(Var, InvalidationReason::Deleted(DE->getSourceRange(),
                                                       CurrentBlock));
      }
    }
  }

  void VisitCXXThrowExpr(const CXXThrowExpr *TE) {
    if (!TE->getSubExpr())
      return;
    if (!isPointer(TE->getSubExpr()))
      return;
    PSet ThrownPSet = getPSet(TE->getSubExpr());
    if (!ThrownPSet.isStatic())
      Reporter.warnNonStaticThrow(TE->getSourceRange(), ThrownPSet.str());
  }

  template <typename PC, typename TC>
  static void forEachArgParamPair(const CallExpr *CE, const PC &ParamCallback,
                                  const TC &ThisCallback) {
    const FunctionDecl *FD = CE->getDirectCallee();
    assert(FD);

    ArrayRef<const Expr *> Args =
        llvm::makeArrayRef(CE->getArgs(), CE->getNumArgs());

    const Expr *ObjectArg = nullptr;
    if (isa<CXXOperatorCallExpr>(CE) && FD->isCXXInstanceMember()) {
      ObjectArg = Args[0];
      Args = Args.slice(1);
    } else if (auto *MCE = dyn_cast<CXXMemberCallExpr>(CE))
      ObjectArg = MCE->getImplicitObjectArgument();

    unsigned Pos = 0;
    for (const Expr *Arg : Args) {
      // This can happen for c style var arg functions
      if (Pos >= FD->getNumParams()) {
        ParamCallback(nullptr, Arg, Pos);
      } else {
        const ParmVarDecl *PVD = FD->getParamDecl(Pos);
        ParamCallback(PVD, Arg, Pos);
      }
      ++Pos;
    }
    if (ObjectArg) {
      const CXXRecordDecl *RD = cast<CXXMethodDecl>(FD)->getParent();
      ThisCallback(Variable::thisPointer(RD), RD, ObjectArg);
    }
  }

  // In the contracts every PSets are expressed in terms of the ParmVarDecls.
  // We need to translate this to the PSets of the arguments so we can check
  // substitutability.
  void bindArguments(PSetsMap &Fill, const PSetsMap &Lookup, const CallExpr *CE,
                     bool Checking = true) {
    // The sources of null are the actuals, not the formals.
    if (!Checking)
      for (auto &VarToPSet : Fill)
        VarToPSet.second.removeNull();

    auto bindTwoDerefLevels = [this, &Lookup,
                               Checking](Variable V, const PSet &PS,
                                         PSetsMap::value_type &Pair) {
      Pair.second.bind(V, PS, Checking);
      if (!Lookup.count(V))
        return;
      V.deref();
      Pair.second.bind(V, derefPSet(PS), Checking);
    };

    auto ReturnIt = Fill.find(Variable::returnVal());
    forEachArgParamPair(
        CE,
        [&](const ParmVarDecl *PVD, const Expr *Arg, int Pos) {
          if (!PVD) {
            // PVD is a c-style vararg argument.
            return;
          }
          PSet ArgPS = getPSet(Arg, /*AllowNonExisting=*/true);
          if (ArgPS.isUnknown()) {
            return;
          }
          Variable V = PVD;
          V.deref();
          for (auto &VarToPSet : Fill)
            bindTwoDerefLevels(V, ArgPS, VarToPSet);
          // Do the binding for the return value.
          if (ReturnIt != Fill.end())
            bindTwoDerefLevels(V, ArgPS, *ReturnIt);
        },
        [&](Variable V, const CXXRecordDecl *, const Expr *ObjExpr) {
          // Do the binding for this and *this
          V.deref();
          for (auto &VarToPSet : Fill)
            bindTwoDerefLevels(V, getPSet(ObjExpr), VarToPSet);
        });
  }

  /// Evaluates the CallExpr for effects on psets.
  /// When a non-const pointer to pointer or reference to pointer is passed
  /// into a function, it's pointee's are invalidated.
  /// Returns true if CallExpr was handled.
  void VisitCallExpr(const CallExpr *CallE) {
    // Default return value, will be overwritten if it makes sense.
    setPSet(CallE, {});

    if (isa<CXXPseudoDestructorExpr>(CallE->getCallee()) ||
        HandleDebugFunctions(CallE))
      return;

    // TODO: function pointers are not handled. We need to get the contracts
    //       from the declaration of the function pointer somehow.
    const auto *Callee = CallE->getDirectCallee();
    if (!Callee)
      return;

    /// Special case for assignment of Pointer into Pointer: copy pset
    if (auto *OC = dyn_cast<CXXOperatorCallExpr>(CallE)) {
      if (OC->getOperator() == OO_Equal && OC->getNumArgs() == 2 &&
          isPointer(OC->getArg(0)) && isPointer(OC->getArg(1))) {
        SourceRange Range = CallE->getSourceRange();
        PSet RHS = getPSet(getPSet(OC->getArg(1)));
        RHS = handlePointerAssign(OC->getArg(0)->getType(), RHS, Range);
        setPSet(getPSet(OC->getArg(0)), RHS, Range);
        setPSet(CallE, RHS);
        return;
      }
    }

    // Get preconditions.
    PSetsMap PreConditions;
    getLifetimeContracts(PreConditions, Callee, ASTCtxt, CurrentBlock,
                         IsConvertible, Reporter, /*Pre=*/true);
    bindArguments(PreConditions, PreConditions, CallE);

    // Check preconditions. We might have them 2 levels deep.
    forEachArgParamPair(
        CallE,
        [&](const ParmVarDecl *PVD, const Expr *Arg, int Pos) {
          PSet ArgPS = getPSet(Arg, /*AllowNonExisting=*/true);
          if (ArgPS.isUnknown())
            return;
          if (!PVD) {
            // PVD is a c-style vararg argument
            if (ArgPS.containsInvalid()) {
              if (!ArgPS.shouldBeFilteredBasedOnNotes(Reporter) ||
                  ArgPS.isInvalid()) {
                Reporter.warnNullDangling(
                    WarnType::Dangling, Arg->getSourceRange(),
                    ValueSource::Param, "", !ArgPS.isInvalid());
                ArgPS.explainWhyInvalid(Reporter);
              }
              setPSet(Arg, PSet()); // Suppress further warnings.
            }
            return;
          }
          if (PreConditions.count(PVD) &&
              !ArgPS.checkSubstitutableFor(PreConditions[PVD],
                                           Arg->getSourceRange(), Reporter))
            setPSet(Arg, PSet()); // Suppress further warnings.
          Variable V = PVD;
          V.deref();
          if (PreConditions.count(V))
            derefPSet(ArgPS).checkSubstitutableFor(
                PreConditions[V], Arg->getSourceRange(), Reporter);
        },
        [&](Variable V, const RecordDecl *, const Expr *ObjExpr) {
          PSet ArgPS = getPSet(ObjExpr);
          if (PreConditions.count(V) &&
              !ArgPS.checkSubstitutableFor(PreConditions[V],
                                           ObjExpr->getSourceRange(), Reporter))
            setPSet(ObjExpr, PSet()); // Suppress further warnings.
          V.deref();
          if (PreConditions.count(V))
            derefPSet(ArgPS).checkSubstitutableFor(
                PreConditions[V], ObjExpr->getSourceRange(), Reporter);
        });

    PSetsMap PostConditions;
    getLifetimeContracts(PostConditions, Callee, ASTCtxt, CurrentBlock,
                         IsConvertible, Reporter, /*Pre=*/false);
    bindArguments(PostConditions, PreConditions, CallE, /*Checking=*/false);
    // PSets might become empty during the argument binding.
    // E.g.: when the pset(null) is bind to a non-null pset.
    // Also remove null outputs for non-null types.
    for (auto &Pair : PostConditions) {
      // TODO: Currently getType() fails when isReturnVal() is true because the
      // Variable does not store the type of the ReturnVal.
      QualType OutputType = Pair.first.isReturnVal() ? Callee->getReturnType()
                                                     : Pair.first.getType();
      if (!isNullableType(OutputType))
        Pair.second.removeNull();
      if (Pair.second.isUnknown())
        Pair.second.addStatic();
    }

#if 0
    for (auto Pair : PreConditions)
      llvm::errs() << "Pre:" << Pair.first.getName() << " -> "
                   << Pair.second.str() << "\n";
    for (auto Pair : PostConditions)
      llvm::errs() << "Post" << Pair.first.getName() << " -> "
                   << Pair.second.str() << "\n";
#endif

    // Invalidate owners taken by Pointer to non-const.
    forEachArgParamPair(
        CallE,
        [&](const ParmVarDecl *PVD, const Expr *Arg, int Pos) {
          if (!PVD) // C-style vararg argument.
            return;
          QualType Pointee = getPointeeType(PVD->getType());
          if (Pointee.isNull())
            return;
          if (classifyTypeCategory(Pointee) != TypeCategory::Owner ||
              isLifetimeConst(Callee, Pointee, Pos))
            return;
          PSet ArgPS = getPSet(Arg);
          for (Variable V : ArgPS.vars())
            invalidateOwner(V, InvalidationReason::Modified(
                                   Arg->getSourceRange(), CurrentBlock));
        },
        [&](Variable, const RecordDecl *RD, const Expr *ObjExpr) {
          const auto *RT = RD->getTypeForDecl();
          if (classifyTypeCategory(RT) != TypeCategory::Owner ||
              isLifetimeConst(Callee, QualType(RT, 0), -1))
            return;
          PSet ArgPs = getPSet(ObjExpr);
          for (Variable V : ArgPs.vars())
            invalidateOwner(V, InvalidationReason::Modified(
                                   ObjExpr->getSourceRange(), CurrentBlock));
        });

    // Bind Pointer return value.
    auto TC = classifyTypeCategory(Callee->getReturnType());
    if (TC == TypeCategory::Pointer)
      setPSet(CallE, PostConditions[Variable::returnVal()]);

    // Bind output arguments.
    forEachArgParamPair(
        CallE,
        [&](const ParmVarDecl *PVD, const Expr *Arg, int Pos) {
          if (!PVD) {
            // C-style vararg argument.
            if (!hasPSet(Arg))
              return;
            PSet ArgPS = getPSet(Arg);
            if (ArgPS.vars().empty())
              return;
            setPSet(ArgPS, PSet::staticVar(false),
                    Arg->getSourceRange());
            return;
          }
          Variable V = PVD;
          V.deref();
          if (PostConditions.count(V))
            setPSet(getPSet(Arg), PostConditions[V], Arg->getSourceRange());
        },
        [&](Variable V, const RecordDecl *RD, const Expr *ObjExpr) {
          V.deref();
          if (PostConditions.count(V))
            setPSet(getPSet(ObjExpr), PostConditions[V],
                    ObjExpr->getSourceRange());
        });

    // Lambdas are not supported yet properly. Invalidate the captured pointers
    // so we do not get false positives with uninitialized values.
    if (const auto *M = dyn_cast<CXXMethodDecl>(Callee)) {
      const CXXRecordDecl *RD = M->getParent();
      if (!RD->isLambda())
        return;
      for (const LambdaCapture &Capture : RD->captures()) {
        if (Capture.getCaptureKind() != LCK_ByRef ||
            !Capture.capturesVariable())
          continue;
        const VarDecl *VD = Capture.getCapturedVar();
        if (classifyTypeCategory(VD->getType()) == TypeCategory::Pointer)
          setPSet(PSet::singleton(VD), {}, Callee->getSourceRange());
      }
    }
  }

  bool CheckPSetValidity(const PSet &PS, SourceRange Range) const;

  void invalidateVar(Variable V, InvalidationReason Reason) {
    for (const auto &I : PMap) {
      const PSet &PS = I.second;
      if (PS.containsInvalid())
        continue; // Nothing to invalidate

      const auto &Var = I.first;
      if (PS.containsParent(V))
        setPSet(PSet::singleton(Var), PSet::invalid(Reason), Reason.getRange());
    }
  }

  void invalidateOwner(Variable V, InvalidationReason Reason) {
    for (const auto &I : PMap) {
      const auto &Var = I.first;
      if (V == Var)
        continue; // Invalidating Owner' should not change the pset of Owner
      const PSet &PS = I.second;
      if (PS.containsInvalid())
        continue; // Nothing to invalidate

      auto DerefV = V;
      DerefV.deref();
      if (PS.containsParent(DerefV))
        setPSet(PSet::singleton(Var), PSet::invalid(Reason), Reason.getRange());
    }
  }

  // Remove the variable from the pset together with the materialized
  // temporaries extended by that variable. It also invalidates the pointers
  // pointing to these.
  // If VD is nullptr, invalidates all psets that contain
  // MaterializeTemporaryExpr without extending decl.
  void eraseVariable(const VarDecl *VD, SourceRange Range) {
    InvalidationReason Reason =
        VD ? InvalidationReason::PointeeLeftScope(Range, CurrentBlock, VD)
           : InvalidationReason::TemporaryLeftScope(Range, CurrentBlock);
    if (VD) {
      PMap.erase(VD);
      invalidateVar(VD, Reason);
    }
    // Remove all materialized temporaries that were extended by this
    // variable (or a lifetime extended temporary without an extending
    // declaration) and do the invalidation.
    for (auto I = PMap.begin(); I != PMap.end();) {
      if (I->first.isTemporaryExtendedBy(VD)) {
        I = PMap.erase(I);
      } else {
        auto &Var = I->first;
        auto &Pset = I->second;
        bool PsetContainsTemporary =
            llvm::any_of(Pset.vars(), [VD](const Variable &V) {
              return V.isTemporaryExtendedBy(VD);
            });
        if (PsetContainsTemporary)
          setPSet(PSet::singleton(Var), PSet::invalid(Reason),
                  Reason.getRange());
        ++I;
      }
    }
  }

  PSet getPSet(Variable P) const;

  PSet getPSet(const Expr *E, bool AllowNonExisting = false) const {
    E = IgnoreTransparentExprs(E);
    if (E->isLValue()) {
      auto I = RefersTo.find(E);
      if (I != RefersTo.end())
        return I->second;
      if (AllowNonExisting)
        return {};
#ifndef NDEBUG
      E->dump();
      llvm_unreachable("Expression has no entry in RefersTo");
#endif
      return {};
    } else {
      auto I = PSetsOfExpr.find(E);
      if (I != PSetsOfExpr.end())
        return I->second;
      if (AllowNonExisting)
        return {};
#ifndef NDEBUG
      E->dump();
      llvm_unreachable("Expression has no entry in PSetsOfExpr");
#endif
      return {};
    }
  }

  PSet getPSet(const PSet &P) const {
    PSet Ret;
    if (P.containsInvalid())
      return PSet::invalid(P.invReasons());

    for (auto &Var : P.vars())
      Ret.merge(getPSet(Var));

    if (P.containsStatic())
      Ret.merge(PSet::staticVar(false));
    return Ret;
  }

  void setPSet(const Expr *E, const PSet &PS) {
    if (E->isLValue()) {
      DBG("Set RefersTo[" << E->getStmtClassName() << "] = " << PS.str()
                          << "\n");
      RefersTo[E] = PS;
    } else {
      DBG("Set PSetsOfExpr[" << E->getStmtClassName() << "] = " << PS.str()
                             << "\n");
      PSetsOfExpr[E] = PS;
    }
  }
  void setPSet(PSet LHS, PSet RHS, SourceRange Range);
  PSet derefPSet(const PSet &P) const;

  bool HandleDebugFunctions(const CallExpr *CallE) const;

  PSet handlePointerAssign(QualType LHS, PSet RHS, SourceRange Range,
                           bool AddReason = true) const {
    if (RHS.containsNull()) {
      if (AddReason)
        RHS.addNullReason(NullReason::assigned(Range, CurrentBlock));
      if (!isNullableType(LHS)) {
        Reporter.warn(WarnType::AssignNull, Range, !RHS.isNull());
        RHS = PSet{};
      }
    }
    return RHS;
  }

  void VisitVarDecl(const VarDecl *VD) {
    // TODO: handle DecompositionDecl.
    const Expr *Initializer = VD->getInit();
    SourceRange Range = VD->getSourceRange();

    switch (classifyTypeCategory(VD->getType())) {
    case TypeCategory::Pointer: {
      PSet PS;
      if (Initializer) {
        // For raw pointers, show here the assignment. For other Pointers,
        // we will have seen a CXXConstructor, which added a NullReason.
        PS = handlePointerAssign(VD->getType(), getPSet(Initializer),
                                 VD->getSourceRange(),
                                 VD->getType()->isPointerType());
      } else if (VD->hasGlobalStorage()) {
        // Never treat local statics as uninitialized.
        PS = PSet::staticVar(/*TODO*/ false);
      } else {
        PS = PSet::invalid(InvalidationReason::NotInitialized(VD->getLocation(),
                                                              CurrentBlock));
      }
      setPSet(PSet::singleton(VD), PS, Range);
      break;
    }
    case TypeCategory::Owner: {
      setPSet(PSet::singleton(VD), PSet::singleton(VD, 1), Range);
      break;
    }
    default:;
    }
  }

  void UpdatePSetsFromCondition(const Stmt *S, bool Positive,
                                llvm::Optional<PSetsMap> &FalseBranchExitPMap,
                                SourceRange Range);

public:
  PSetsBuilder(const FunctionDecl *FD, LifetimeReporterBase &Reporter,
               ASTContext &ASTCtxt, PSetsMap &PMap,
               std::map<const Expr *, PSet> &PSetsOfExpr,
               std::map<const Expr *, PSet> &RefersTo,
               IsConvertibleTy IsConvertible)
      : AnalyzedFD(FD), Reporter(Reporter), ASTCtxt(ASTCtxt),
        IsConvertible(IsConvertible), PMap(PMap), PSetsOfExpr(PSetsOfExpr),
        RefersTo(RefersTo) {}

  void VisitBlock(const CFGBlock &B,
                  llvm::Optional<PSetsMap> &FalseBranchExitPMap);
}; // namespace
} // namespace

// Manages lifetime information for the CFG of a FunctionDecl
PSet PSetsBuilder::getPSet(Variable P) const {
  // Assumption: global Pointers have a pset of {static}
  if (P.hasStaticLifetime())
    return PSet::staticVar(false);

  auto I = PMap.find(P);
  if (I != PMap.end())
    return I->second;

  // Assume that the unseen pointer fields are valid. We will always have
  // unseen fields since we do not track the fields of owners and values.
  // Until proper aggregate support is implemented, this might be triggered
  // unintentionally.
  if (P.isField())
    return PSet::staticVar(false);

  if (P.getType()->isArrayType()) {
    // This triggers when we have an array of Pointers, and we
    // do a subscript to obtain a Pointer.
    // TODO: We currently have no idea what that Pointer may point at.
    // The array itself should have a pset. It starts with (invalid) if there
    // is not initialization done. Whenever there is an assignment to any array
    // member, the pset of the array should grow by the pset of the assigment
    // LHS. (This means that an uninitialized array can never become valid. This
    // is reasonable, because we have no way to track if all array elements have
    // been set at some later point.)
    return {};
  }

  if (auto VD = P.asVarDecl()) {
    // Handle goto_forward_over_decl() in test attr-pset.cpp
    if (!isa<ParmVarDecl>(VD))
      return PSet::invalid(
          InvalidationReason::NotInitialized(VD->getLocation(), CurrentBlock));
  }

  return {};
}

/// Computes the pset of dereferencing a variable with the given pset
/// If PS contains (null), it is silently ignored.
PSet PSetsBuilder::derefPSet(const PSet &PS) const {
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

  for (auto V : PS.vars()) {
    int Order = V.getOrder();
    if (Order > 0) {
      if (Order > MaxOrderDepth)
        RetPS.addStatic();
      else
        RetPS.insert(V.deref()); // pset(o') = { o'' }
    } else
      RetPS.merge(getPSet(V));
  }

  return RetPS;
}

void PSetsBuilder::setPSet(PSet LHS, PSet RHS, SourceRange Range) {
  // Assumption: global Pointers have a pset that is a subset of {static,
  // null}
  if (LHS.isStatic() && !RHS.isUnknown() && !RHS.isStatic() && !RHS.isNull()) {
    StringRef SourceText =
        Lexer::getSourceText(CharSourceRange::getTokenRange(Range),
                             ASTCtxt.getSourceManager(), ASTCtxt.getLangOpts());
    Reporter.warnPsetOfGlobal(Range, SourceText, RHS.str());
  }

  DBG("PMap[" << LHS.str() << "] = " << RHS.str() << "\n");
  if (LHS.vars().size() == 1) {
    Variable Var = *LHS.vars().begin();
    RHS.addReasonTarget(Var);
    auto I = PMap.find(Var);
    if (I != PMap.end())
      I->second = std::move(RHS);
    else
      PMap.emplace(Var, RHS);
  } else {
    for (auto &V : LHS.vars()) {
      auto I = PMap.find(V);
      if (I != PMap.end())
        I->second.merge(RHS);
      else
        PMap.emplace(V, RHS);
    }
  }
}

bool PSetsBuilder::CheckPSetValidity(const PSet &PS, SourceRange Range) const {
  if (PS.containsInvalid()) {
    if (PS.shouldBeFilteredBasedOnNotes(Reporter) && !PS.isInvalid())
      return false;
    Reporter.warn(WarnType::DerefDangling, Range, !PS.isInvalid());
    PS.explainWhyInvalid(Reporter);
    return false;
  }

  if (PS.containsNull()) {
    if (PS.shouldBeFilteredBasedOnNotes(Reporter) && !PS.isNull())
      return false;
    Reporter.warn(WarnType::DerefNull, Range, !PS.isNull());
    PS.explainWhyNull(Reporter);
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

  auto TC = classifyTypeCategory(E->getType());
  if (E->isLValue() &&
      (TC == TypeCategory::Pointer || TC == TypeCategory::Owner)) {
    auto Ref = getPSet(E);
    // We refer to multiple variables (or none),
    // and we cannot know which of them is null/non-null.
    if (Ref.vars().size() != 1)
      return;

    Variable V = *Ref.vars().begin();
    Variable DerefV = V;
    DerefV.deref();
    PSet PS = getPSet(V);
    PSet PSElseBranch = PS;
    FalseBranchExitPMap = PMap;
    if (Positive) {
      // The variable is non-null in the if-branch and null in the then-branch.
      PSElseBranch.removeEverythingButNull();
      PS.removeNull();
      auto It = FalseBranchExitPMap->find(DerefV);
      if (It != FalseBranchExitPMap->end())
        It->second = PSet();
    } else {
      // The variable is null in the if-branch and non-null in the then-branch.
      PS.removeEverythingButNull();
      PSElseBranch.removeNull();
      auto It = PMap.find(DerefV);
      if (It != PMap.end())
        It->second = PSet();
    }
    (*FalseBranchExitPMap)[V] = PSElseBranch;
    setPSet(PSet::singleton(V), PS, Range);
  }
} // namespace lifetime

/// Checks if the statement S is a call to a debug function and dumps the
/// corresponding part of the state.
bool PSetsBuilder::HandleDebugFunctions(const CallExpr *CallE) const {

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
                     .Case("__lifetime_contracts", 5)
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
    Reporter.debugPset(Range, SourceText, Set.str());
    return true;
  }
  case 3: {
    auto Args = Callee->getTemplateSpecializationArgs();
    auto QType = Args->get(0).getAsType();
    auto Class = classifyTypeCategory(QType);
    if (Class.TC == TypeCategory::Pointer || Class.TC == TypeCategory::Owner) {
      Reporter.debugTypeCategory(Range, Class.TC,
                                 Class.PointeeType.getAsString());
    } else {
      Reporter.debugTypeCategory(Range, Class.TC);
    }
    return true;
  }
  case 4: {
    auto QType = CallE->getArg(0)->getType();
    auto Class = classifyTypeCategory(QType);
    if (Class.TC == TypeCategory::Pointer || Class.TC == TypeCategory::Owner) {
      Reporter.debugTypeCategory(Range, Class.TC,
                                 Class.PointeeType.getAsString());
    } else {
      Reporter.debugTypeCategory(Range, Class.TC);
    }
    return true;
  }
  case 5: {
    const Expr *E = CallE->getArg(0)->IgnoreImplicit();
    if (const auto *UO = dyn_cast<UnaryOperator>(E))
      E = UO->getSubExpr();
    const auto *FD = dyn_cast<FunctionDecl>(cast<DeclRefExpr>(E)->getDecl());
    FD = FD->getCanonicalDecl();
    const auto LAttr = FD->getAttr<LifetimeContractAttr>();
    auto printContract =
        [FD, this,
         &Range](LifetimeContractAttr::PointsToMap::iterator::value_type E,
                 const std::string &Contract) {
          std::string KeyText = Contract + "(";
          KeyText += Variable(E.first, FD).getName();
          KeyText += ")";
          std::string PSetText = PSet(E.second, FD).str();
          Reporter.debugPset(Range, KeyText, PSetText);
        };
    for (const auto &E : LAttr->PrePSets)
      printContract(E, "Pre");
    for (const auto &E : LAttr->PostPSets)
      printContract(E, "Post");
    return true;
  }
  default:
    llvm_unreachable("Unknown debug function.");
  }
} // namespace lifetime

static const Stmt *getRealTerminator(const CFGBlock &B) {
  // For expressions like (bool)(p && q), q will only have one successor,
  // the cast operation. But we still want to compute two sets for q so
  // we can propagate this information through the cast.
  if (B.succ_size() == 1 && !B.empty() &&
      B.rbegin()->getKind() == CFGElement::Kind::Statement) {
    auto Succ = B.succ_begin()->getReachableBlock();
    if (Succ && isNoopBlock(*Succ) && Succ->succ_size() == 2)
      return B.rbegin()->castAs<CFGStmt>().getStmt();
  }

  return B.getLastCondition();
}

static bool isThrowingBlock(const CFGBlock &B) {
  return llvm::any_of(B, [](const CFGElement &E) {
    return E.getKind() == CFGElement::Statement &&
           isa<CXXThrowExpr>(E.getAs<CFGStmt>()->getStmt());
  });
}

static SourceRange getSourceRange(const CFGElement &E) {
  if (llvm::Optional<CFGStmt> S = E.getAs<CFGStmt>())
    return S->getStmt()->getSourceRange();
  else if (llvm::Optional<CFGLifetimeEnds> S = E.getAs<CFGLifetimeEnds>())
    return S->getTriggerStmt()->getSourceRange();
  return {};
}

// Update PSets in Builder through all CFGElements of this block
void PSetsBuilder::VisitBlock(const CFGBlock &B,
                              llvm::Optional<PSetsMap> &FalseBranchExitPMap) {
  CurrentBlock = &B;
  for (const auto &E : B) {
    switch (E.getKind()) {
    case CFGElement::Statement: {
      const Stmt *S = E.castAs<CFGStmt>().getStmt();
      if (!IsIgnoredStmt(S)) {
        Visit(S);
        if (isAnalysisDisabled())
          return;
#ifndef NDEBUG
        if (auto *Ex = dyn_cast<Expr>(S)) {
          if (Ex->isLValue() && !Ex->getType()->isFunctionType() &&
              RefersTo.find(Ex) == RefersTo.end()) {
            Ex->dump();
            llvm_unreachable("Missing entry in RefersTo");
          }
          if (!Ex->isLValue() && hasPSet(Ex) &&
              PSetsOfExpr.find(Ex) == PSetsOfExpr.end()) {
            Ex->dump();
            llvm_unreachable("Missing entry in PSetsOfExpr");
          }
        }
#endif
      }
      /*llvm::errs() << "TraverseStmt\n";
      S->dump();
      llvm::errs() << "\n";*/

      // Kill all temporaries that vanish at the end of the full expression
      if (isa<ExprWithCleanups>(S) || isa<DeclStmt>(S)) {
        // Remove all materialized temporaries that are not extended.
        eraseVariable(nullptr, S->getEndLoc());
      }
      if (!isa<Expr>(S)) {
        // Clean up P- and RefersTo-sets for subexpressions.
        // We should never reference subexpressions again after
        // the full expression ended. The problem is,
        // it is not trivial to find out the end of a full
        // expression with linearized CFGs.
        // This is why currently the sets are only cleared for
        // statements which are not expressions.
        // TODO: clean this up by properly tracking end of full exprs.
        RefersTo.clear();
        PSetsOfExpr.clear();
      }

      break;
    }
    case CFGElement::LifetimeEnds: {
      auto Leaver = E.castAs<CFGLifetimeEnds>();

      // Stop tracking Variables that leave scope.
      eraseVariable(Leaver.getVarDecl(), Leaver.getTriggerStmt()->getEndLoc());
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
                             Terminator->getEndLoc());
  }
  if (B.hasNoReturnElement() || isThrowingBlock(B))
    return;
  if (B.succ_size() == 1 && *B.succ_begin() == &B.getParent()->getExit()) {
    PSetsMap PostConditions;
    getLifetimeContracts(PostConditions, AnalyzedFD, ASTCtxt, &B, IsConvertible,
                         Reporter, /*Pre=*/false);
    for (auto &VarToPSet : PostConditions) {
      if (VarToPSet.first.isReturnVal())
        continue;
      auto OutVarIt = PMap.find(VarToPSet.first);
      assert(OutVarIt != PMap.end());
      OutVarIt->second.checkSubstitutableFor(
          VarToPSet.second, getSourceRange(B.back()), Reporter,
          ValueSource::OutputParam, VarToPSet.first.getName());
    }
  }
} // namespace lifetime

bool VisitBlock(const FunctionDecl *FD, PSetsMap &PMap,
                llvm::Optional<PSetsMap> &FalseBranchExitPMap,
                std::map<const Expr *, PSet> &PSetsOfExpr,
                std::map<const Expr *, PSet> &RefersTo, const CFGBlock &B,
                LifetimeReporterBase &Reporter, ASTContext &ASTCtxt,
                IsConvertibleTy IsConvertible) {
  Reporter.setCurrentBlock(&B);
  PSetsBuilder Builder(FD, Reporter, ASTCtxt, PMap, PSetsOfExpr, RefersTo,
                       IsConvertible);
  Builder.VisitBlock(B, FalseBranchExitPMap);
  return !Builder.isAnalysisDisabled();
}
} // namespace lifetime
} // namespace clang
