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
#include "clang/Analysis/Analyses/Lifetime.h"
#include "clang/Analysis/CFG.h"
#include "clang/Sema/SemaDiagnostic.h" // TODO: remove me and move all diagnostics into LifetimeReporter

namespace clang {
namespace lifetime {

// Collection of methods to update/check PSets from statements/expressions
class PSetsBuilder {

  const LifetimeReporterBase *Reporter;
  ASTContext &ASTCtxt;
  PSetsMap &PSets;

  /// IgnoreParenImpCasts - Ignore parentheses and implicit casts.  Strip off
  /// any ParenExpr
  /// or ImplicitCastExprs, returning their operand.
  /// Does not ignore MaterializeTemporaryExpr as Expr::IgnoreParenImpCasts
  /// would.
  static const Expr *IgnoreParenImpCasts(const Expr *E) {
    while (true) {
      E = E->IgnoreParens();
      if (const auto *P = dyn_cast<ImplicitCastExpr>(E)) {
        E = P->getSubExpr();
        continue;
      }
      return E;
    }
  }

  // Returns the variable which the expression refers to,
  // or None.
  Variable refersTo(const Expr *E) {
    E = E->IgnoreParenCasts();
    switch (E->getStmtClass()) {
    case Expr::DeclRefExprClass: {
      const auto *DeclRef = cast<DeclRefExpr>(E);
      auto *VD = dyn_cast<VarDecl>(DeclRef->getDecl());
      if (VD)
        return Variable(VD);
      return Variable::temporary();
    }

    case Expr::CXXThisExprClass: {
      return Variable::thisPointer();
    }

    case Expr::MemberExprClass: {
      const auto *M = cast<MemberExpr>(E);
      auto SubV = refersTo(M->getBase());

      /// The returned declaration will be a FieldDecl or (in C++) a VarDecl
      /// (for static data members), a CXXMethodDecl, or an EnumConstantDecl.
      auto *FD = dyn_cast<FieldDecl>(M->getMemberDecl());
      if (FD) {
        SubV.addFieldRef(FD);
        return SubV;
      }
      return Variable::temporary();
    }
    default:
      return Variable::temporary();
    }
  }

  // Evaluate the given expression for all effects on psets of variables
  void EvalExpr(const Expr *E) {
    E = E->IgnoreParenCasts();

    switch (E->getStmtClass()) {
    case Expr::BinaryOperatorClass: {
      const auto *BinOp = cast<BinaryOperator>(E);
      if (BinOp->getOpcode() == BO_Assign) {
        EvalExpr(BinOp->getLHS()); // Eval for side-effects
        auto Category = classifyTypeCategory(BinOp->getLHS()->getType());
        Variable V = refersTo(BinOp->getLHS());
        if (Category == TypeCategory::Owner) {
          // When an Owner x is copied to or moved to, set pset(x) = {x'}
          SetPSet(V, PSet::pointsToVariable(V, false, 1), BinOp->getExprLoc());
        } else if (Category == TypeCategory::Pointer) {
          // This assignment updates a Pointer
          PSet PS = EvalExprForPSet(BinOp->getRHS(), false);
          SetPSet(V, PS, BinOp->getExprLoc());
        }
        return;
      } else if (BinOp->getLHS()->getType()->isPointerType() &&
                 (BinOp->getOpcode() == BO_AddAssign ||
                  BinOp->getOpcode() == BO_SubAssign)) {
        // Affects pset; forbidden by the bounds profile.
        Variable V = refersTo(BinOp->getLHS());
        SetPSet(V,
                PSet::invalid(
                    InvalidationReason::PointerArithmetic(BinOp->getExprLoc())),
                BinOp->getExprLoc());
        // TODO: diagnose even if we have no VarDecl?
      }
      break;
    }
    case Expr::UnaryOperatorClass: {
      const auto *UnOp = cast<UnaryOperator>(E);
      auto *SubExpr = UnOp->getSubExpr();
      if (SubExpr->getType()->isPointerType()) {
        switch (UnOp->getOpcode()) {
        case UO_Deref: {
          // Check if dereferencing this pointer is valid
          PSet PS = EvalExprForPSet(SubExpr, false);
          CheckPSetValidity(PS, UnOp->getExprLoc());
          return;
        }
        case UO_PostInc:
        case UO_PostDec:
        case UO_PreInc:
        case UO_PreDec: {
          // Affects pset; forbidden by the bounds profile.
          if (Optional<Variable> V = refersTo(SubExpr)) {
            SetPSet(*V,
                    PSet::invalid(InvalidationReason::PointerArithmetic(
                        UnOp->getExprLoc())),
                    UnOp->getExprLoc());
          }
          // TODO: diagnose even if we have no VarDecl?
          break;
        }
        default:
          break; // Traversing is done below
        }
      }
      break;
    }
    case Expr::MemberExprClass: {
      const auto *MemberE = cast<MemberExpr>(E);
      const Expr *Base = MemberE->getBase();
      // 'this' can never dangle
      // TODO: check pset for !isArrow if Base is a reference
      if (!isa<CXXThisExpr>(Base) && MemberE->isArrow()) {
        PSet PS = EvalExprForPSet(Base, false);
        CheckPSetValidity(PS, MemberE->getExprLoc());
        return;
      }
      break;
    }
    case Expr::CXXOperatorCallExprClass:
    case Expr::CXXMemberCallExprClass:
    case Expr::CallExprClass: {
      EvalCallExpr(cast<CallExpr>(E));
      return;
    }
    case Expr::ConditionalOperatorClass:
      // Do not handle it here; this is always the terminator of a CFG block
      // and it will be handled explicitly there.
      return;
    default:;
    }
    // Other Expr, just recurse
    for (const Stmt *SubStmt : E->children())
      EvalStmt(SubStmt);
  }

  struct CallArgument {
    CallArgument(const Expr *ArgumentExpr, PSet PS)
        : ArgumentExpr(ArgumentExpr), PS(std::move(PS)) {}
    const Expr *ArgumentExpr;
    PSet PS;
  };

  struct CallArguments {
    std::vector<CallArgument> Oin_strong;
    std::vector<CallArgument> Oin_weak;
    std::vector<CallArgument> Oin;
    std::vector<CallArgument> Pin;
    // The pointee is the output
    std::vector<const Expr *> Pout;
  };

  void PushCallArguments(const Expr *Arg, QualType ParamQType,
                         CallArguments &Args) {
    // TODO implement gsl::lifetime annotations
    // TODO implement aggregates
    const Type *ParamType = ParamQType->getUnqualifiedDesugaredType();

    if (auto *R = dyn_cast<ReferenceType>(ParamType)) {
      if (classifyTypeCategory(R->getPointeeType()) == TypeCategory::Owner) {
        if (isa<RValueReferenceType>(R)) {
          Args.Oin_strong.emplace_back(
              Arg, EvalExprForPSet(Arg, /*referenceCtx=*/false));
        } else if (ParamQType.isConstQualified()) {
          Args.Oin_weak.emplace_back(
              Arg, EvalExprForPSet(Arg, /*referenceCtx=*/false));
        } else {
          Args.Oin.emplace_back(Arg,
                                EvalExprForPSet(Arg, /*referenceCtx=*/false));
        }
      } else {
        // Type Category is Pointer due to raw references.
        Args.Pin.emplace_back(Arg, EvalExprForPSet(Arg, /*referenceCtx=*/true));
      }

    } else if (classifyTypeCategory(ParamQType) == TypeCategory::Pointer) {
      Args.Pin.emplace_back(Arg, EvalExprForPSet(Arg, /*referenceCtx=*/false));
    } else {
      EvalExpr(Arg);
    }

    // A “function output” means a return value or a parameter passed by raw
    // pointer to non-const (and is not considered to include the top-level raw
    // pointer, because the output is the pointee).
    if (ParamQType->isPointerType()) {
      auto Pointee = ParamType->getPointeeType();
      if (!Pointee.isConstQualified() &&
          classifyTypeCategory(Pointee) == TypeCategory::Pointer)
        Args.Pout.push_back(Arg);
    }
  }

  struct CallTypes {
    const FunctionProtoType *FTy = nullptr;
    const CXXRecordDecl *ClassDecl = nullptr;
  };

  /// Obtains the function prototype (without 'this' pointer) and the type of
  /// the object (if MemberCallExpr).
  CallTypes GetCallTypes(const Expr *CalleeE) {
    CallTypes CT;

    if (CalleeE->hasPlaceholderType(BuiltinType::BoundMember)) {
      CalleeE = CalleeE->IgnoreParenImpCasts();
      if (const auto *BinOp = dyn_cast<BinaryOperator>(CalleeE)) {
        auto MemberPtr =
            BinOp->getRHS()->getType()->castAs<MemberPointerType>();
        CT.FTy = dyn_cast<FunctionProtoType>(
            MemberPtr->getPointeeType().IgnoreParens().getTypePtr());
        CT.ClassDecl = MemberPtr->getClass()->getAsCXXRecordDecl();
        assert(CT.FTy);
        assert(CT.ClassDecl);
        return CT;
      }

      if (const auto *ME = dyn_cast<MemberExpr>(CalleeE)) {
        CT.FTy = dyn_cast<FunctionProtoType>(ME->getMemberDecl()->getType());
        auto ClassType = ME->getBase()->getType();
        if (ClassType->isPointerType())
          ClassType = ClassType->getPointeeType();
        CT.ClassDecl = ClassType->getAsCXXRecordDecl();
        assert(CT.FTy);
        assert(CT.ClassDecl);
        return CT;
      }

      CalleeE->dump();
      llvm_unreachable("not a binOp after boundMember");
    }

    auto *P = dyn_cast<PointerType>(
        CalleeE->getType()->getUnqualifiedDesugaredType());
    assert(P);
    CT.FTy = dyn_cast<FunctionProtoType>(
        P->getPointeeType()->getUnqualifiedDesugaredType());

    assert(CT.FTy);
    return CT;
  }

  /// Returns the psets of each expressions in PinArgs,
  /// plus the psets of dereferencing each pset further.
  std::vector<CallArgument>
  diagnoseAndExpandPin(const std::vector<CallArgument> &PinArgs) {
    std::vector<CallArgument> PinExtended;
    // llvm::errs() << "Start Pin: ";
    for (auto &CA : PinArgs) {
      const Expr *ArgExpr = CA.ArgumentExpr;
      PSet PS = CA.PS;
      PinExtended.emplace_back(ArgExpr, PS);

      if (PS.containsInvalid()) {
        if (Reporter) {
          Reporter->warnParameterDangling(ArgExpr->getExprLoc(),
                                          /*indirectly=*/false);
          PS.explainWhyInvalid(*Reporter);
        }
        break;
      }
      // For each Pointer parameter p, treat it as if it were additionally
      // followed by a generated deref__p parameter of the same type as *p. If
      // deref_p is also a Pointer, treat it as if it were additionally followed
      // by a generated deref__deref__p parameter of type **p. These are treated
      // as distinct parameters in the following.
      // TODO: What is the type of `*p` when p is of class-type, possibly
      // without operator*?
      QualType QT = ArgExpr->getType()->getPointeeType();
      // llvm::errs() << "param with PSET " << PS.str() << "\n";
      while (!QT.isNull() && QT->isPointerType() && !PS.containsInvalid()) {
        PS = derefPSet(PS, ArgExpr->getExprLoc());
        if (PS.containsInvalid()) {
          if (Reporter) {
            Reporter->warnParameterDangling(ArgExpr->getExprLoc(),
                                            /*indirectly=*/true);
            PS.explainWhyInvalid(*Reporter);
          }
          break;
        }
        // llvm::errs() << " deref -> " << PS.str() << "\n";
        PinExtended.emplace_back(ArgExpr, PS);
        QT = QT->getPointeeType();
      }
    }
    return PinExtended;
  }

  /// Diagnose if psets arguments in Oin and Pin refer to the same variable
  void diagnoseParameterAliasing(const std::vector<CallArgument> &Pin,
                                 const std::vector<CallArgument> &Oin) {
    if (!Reporter)
      return;
    std::map<Variable, SourceLocation> AllVars;
    for (auto &CA : Pin) {
      for (auto &KV : CA.PS.vars()) {
        auto &Var = KV.first;
        // pset(argument(p)) and pset(argument(x)) must be disjoint (as long as
        // not annotated)
        auto i = AllVars.emplace(Var, CA.ArgumentExpr->getExprLoc());
        if (!i.second) {
          Reporter->warnParametersAlias(CA.ArgumentExpr->getExprLoc(),
                                        i.first->second, Var.getName());
        }
      }
    }
    for (auto &CA : Oin) {
      for (auto &KV : CA.PS.vars()) {
        auto &Var = KV.first;
        // pset(argument(p)) and pset(argument(x)) must be disjoint (as long
        // as not annotated) Enforce that pset() of each argument does not
        // refer to a local Owner in Oin
        auto i = AllVars.emplace(Var, CA.ArgumentExpr->getExprLoc());
        if (!i.second) {
          Reporter->warnParametersAlias(CA.ArgumentExpr->getExprLoc(),
                                        i.first->second, Var.getName());
        }
      }
    }

    /*llvm::errs() << "Vars passed into call: ";
    for (auto &KV : AllVars) {
      llvm::errs() << " " << KV.first.getName();
    }
    llvm::errs() << "\n";*/
  }

  /// Evaluates the CallExpr for effects on psets.
  /// When a non-const pointer to pointer or reference to pointer is passed
  /// into a function, it's pointee's are invalidated.
  /// Returns true if CallExpr was handled.
  PSet EvalCallExpr(const CallExpr *CallE) {
    // Handle call to clang_analyzer_pset, which will print the pset of its
    // argument
    if (HandleClangAnalyzerPset(CallE))
      return {};

    auto *CalleeE = CallE->getCallee();
    EvalExpr(CalleeE);
    CallTypes CT = GetCallTypes(CalleeE);
    CallArguments Args;
    for (unsigned i = 0; i < CallE->getNumArgs(); ++i) {
      const Expr *Arg = CallE->getArg(i);

      QualType ParamQType = [&] {
        // For CXXOperatorCallExpr, getArg(0) is the 'this' pointer.
        if (isa<CXXOperatorCallExpr>(CallE)) {
          if (i == 0) {
            auto QT = ASTCtxt.getLValueReferenceType(Arg->getType(),
                                                     /*SpelledAsLValue=*/true);
            if (CT.FTy->isConst())
              QT.addConst();
            return QT;
          } else
            return CT.FTy->getParamType(i - 1);
        }
        if (CT.FTy->isVariadic())
          return Arg->getType();
        else
          return CT.FTy->getParamType(i);
      }();

      PushCallArguments(Arg, ParamQType,
                        Args); // appends into Args
    }

    if (CT.ClassDecl) {
      // A this pointer parameter is treated as if it were declared as a
      // reference to the current object
      if (const auto *MemberCall = dyn_cast<CXXMemberCallExpr>(CallE)) {
        auto *Object = MemberCall->getImplicitObjectArgument();
        assert(Object);

        // TODO: *this is gsl::non_null annotated
        auto LValRefToObject = ASTCtxt.getLValueReferenceType(
            Object->getType(), /*SpelledAsLValue=*/true);
        if (CT.FTy->isConst())
          LValRefToObject.addConst();

        PushCallArguments(Object, LValRefToObject,
                          Args); // appends into Args
      }
    }

    // TODO If p is annotated [[gsl::lifetime(x)]], then ensure that pset(p)
    // == pset(x)

    std::vector<CallArgument> PinExtended = diagnoseAndExpandPin(Args.Pin);
    diagnoseParameterAliasing(PinExtended, Args.Oin);

    // If p is explicitly lifetime-annotated with x, then each call site
    // enforces the precondition that argument(p) is a valid Pointer and
    // pset(argument(p)) == pset(argument(x)), and in the callee on function
    // entry set pset(p) = pset(x).

    // Enforce that pset() of each argument does not refer to a non-const
    // global Owner
    PSet ret;
    for (CallArgument &CA : PinExtended) {
      // llvm::errs() << " Pin PS:" << CA.PS.str() << "\n";
      ret.merge(CA.PS);
    }
    for (CallArgument &CA : Args.Oin) {
      // llvm::errs() << " Oin PS:" << CA.PS.str() << "\n";
      ret.merge(CA.PS);
    }
    return ret;
  }

  /// Evaluates E for effects that change psets.
  /// 1) if referenceCtx is true, returns the pset of 'p' in 'auto &p =
  /// E';
  /// 2) if referenceCtx is false, returns the pset of 'p' in 'auto *p =
  /// E';
  ///
  /// We use pset_ptr(E) to denote ExprPset when referenceCtx is true
  /// and pset_ref(E) to denote ExprPset when referenceTxt is false.
  /// We use pset(v) when v is a VarDecl to refer to the entry of v in the
  /// PSets
  /// map.
  PSet EvalExprForPSet(const Expr *E, bool referenceCtx) {
    E = IgnoreParenImpCasts(E);

    switch (E->getStmtClass()) {
    case Expr::ExprWithCleanupsClass: {
      const auto *EC = cast<ExprWithCleanups>(E);
      EvalExpr(EC->getSubExpr());
      return EvalExprForPSet(EC->getSubExpr(), referenceCtx);
    }
    case Expr::MaterializeTemporaryExprClass: {
      const auto *MaterializeTemporaryE = cast<MaterializeTemporaryExpr>(E);
      assert(referenceCtx);
      EvalExpr(MaterializeTemporaryE->GetTemporaryExpr());

      if (MaterializeTemporaryE->getExtendingDecl())
        return PSet::pointsToVariable(MaterializeTemporaryE, false, 0);
      else
        return PSet::pointsToVariable(Variable::temporary(), false, 0);
    }
    case Expr::DeclRefExprClass: {
      const auto *DeclRef = cast<DeclRefExpr>(E);
      const auto *VD = dyn_cast<VarDecl>(DeclRef->getDecl());
      if (VD) {
        if (referenceCtx) {
          // T i; T &j = i; auto &p = j; -> pset_ref(p) = {i}
          if (VD->getType()->isReferenceType())
            return GetPSet(VD);
          else
            // T i; auto &p = i; -> pset_ref(p) = {i}
            return PSet::pointsToVariable(VD, false);
        } else {
          if (VD->getType()->isArrayType())
            // Forbidden by bounds profile
            // Alternative would be: T a[]; auto *p = a; -> pset_ptr(p) =
            // {a}
            return PSet::invalid(
                InvalidationReason::PointerArithmetic(DeclRef->getLocation()));
          else
            // T i; auto *p = i; -> pset_ptr(p) = pset(i)
            return GetPSet(VD);
        }
      }
      return {};
    }
    case Expr::ArraySubscriptExprClass: {
      const auto *ArraySubscriptE = cast<ArraySubscriptExpr>(E);
      EvalExpr(ArraySubscriptE->getBase());
      EvalExpr(ArraySubscriptE->getIdx());

      // By the bounds profile, ArraySubscriptExpr is only allowed on arrays
      // (not on pointers),
      // thus the base needs to be a DeclRefExpr.
      const auto *DeclRef = dyn_cast<DeclRefExpr>(
          ArraySubscriptE->getBase()->IgnoreParenImpCasts());
      if (DeclRef) {
        const VarDecl *VD = dyn_cast<VarDecl>(DeclRef->getDecl());
        if (VD && VD->getType().getCanonicalType()->isArrayType() &&
            referenceCtx)
          // T a[3]; -> pset_ref(a[i]) = {a}
          return PSet::pointsToVariable(VD, false, 0);
      }
      return {};
    }
    case Expr::ConditionalOperatorClass: {
      const auto *CO = cast<ConditionalOperator>(E);
      // pset_ref(b ? a : b) = pset_ref(a) union pset_ref(b)
      // pset_ptr(b ? a : b) = pset_ptr(a) union pset_ptr(b)
      EvalExpr(CO->getCond());

      PSet PSLHS = EvalExprForPSet(CO->getLHS(), referenceCtx);
      PSet PSRHS = EvalExprForPSet(CO->getRHS(), referenceCtx);
      PSRHS.merge(PSLHS);
      return PSRHS;
    }
    case Expr::CXXThisExprClass: {
      return PSet::pointsToVariable(Variable::thisPointer(), false, 0);
    }
    case Expr::MemberExprClass: {
      const auto *MemberE = cast<MemberExpr>(E);
      const Expr *Base = MemberE->getBase();
      // A static data member of this class
      if (isa<VarDecl>(MemberE->getMemberDecl())) {
        return PSet::staticVar(false);
      }
      PSet RetPSet = EvalExprForPSet(
          Base, !Base->getType().getCanonicalType()->isPointerType());
      if (auto *FD = dyn_cast<FieldDecl>(MemberE->getMemberDecl()))
        RetPSet.addFieldRef(FD);
      return RetPSet;
    }
    case Expr::BinaryOperatorClass: {
      const auto *BinOp = cast<BinaryOperator>(E);
      if (BinOp->getOpcode() == BO_Assign) {
        EvalExpr(BinOp);
        return EvalExprForPSet(BinOp->getLHS(), referenceCtx);
      }
      break;
    }
    case Expr::UnaryOperatorClass: {
      const auto *UnOp = cast<UnaryOperator>(E);
      if (UnOp->getOpcode() == UO_Deref) {
        PSet PS = EvalExprForPSet(UnOp->getSubExpr(), false);
        CheckPSetValidity(PS, UnOp->getOperatorLoc());
        if (referenceCtx) {
          // pset_ref(*p) = pset_ptr(p)
          return PS;
        } else {
          return derefPSet(PS, UnOp->getOperatorLoc());
        }
      } else if (UnOp->getOpcode() == UO_AddrOf) {
        assert(!referenceCtx);
        return EvalExprForPSet(UnOp->getSubExpr(), true);
      }
      break;
    }
    case Expr::CXXReinterpretCastExprClass:
    case Expr::CStyleCastExprClass: {
      const auto *CastE = cast<CastExpr>(E);
      switch (CastE->getCastKind()) {
      case CK_BitCast:
      case CK_LValueBitCast:
      case CK_IntegralToPointer:
        // Those casts are forbidden by the type profile
        // TODO: diagnose
        return {};
      default:
        return EvalExprForPSet(CastE->getSubExpr(), referenceCtx);
      }
    }
    case Expr::InitListExprClass: {
      const auto *I = cast<InitListExpr>(E);
      if (I->isSyntacticForm())
        I = I->getSemanticForm();

      if (I->getType()->isPointerType() && I->getNumInits() == 0)
        return PSet::null(I->getLocStart());

      if (I->getNumInits() == 1)
        return EvalExprForPSet(I->getInit(0), referenceCtx);

      return {};
    }
    case Expr::CXXConstructExprClass:
      return PSet::null(E->getExprLoc());

    case Expr::CXXOperatorCallExprClass:
    case Expr::CXXMemberCallExprClass:
    case Expr::CallExprClass: {
      return EvalCallExpr(cast<CallExpr>(E));
    }
    default: break;
    }

    if (E->isNullPointerConstant(ASTCtxt, Expr::NPC_ValueDependentIsNotNull)) {
      if (referenceCtx) {
        // It is illegal to bind a reference to null
        return PSet::invalid(
            InvalidationReason::NotInitialized(E->getExprLoc()));
      } else {
        return PSet::null(E->getExprLoc());
      }
    }

    // Unhandled case
    EvalExpr(E);
    return {};
  }

  void CheckPSetValidity(const PSet &PS, SourceLocation Loc,
                         bool flagNull = true);

  // Traverse S in depth-first post-order and evaluate all effects on psets
  void EvalStmt(const Stmt *S) {
    if (const auto *DS = dyn_cast<DeclStmt>(S)) {
      assert(DS->isSingleDecl());
      const Decl *D = DS->getSingleDecl();
      if (const auto *VD = dyn_cast<VarDecl>(D))
        EvalVarDecl(VD);
    } else if (const auto *E = dyn_cast<Expr>(S))
      EvalExpr(E);
  }

  /// Invalidates all psets that point to V or something owned by V
  void invalidateOwner(Variable O, unsigned order, InvalidationReason Reason) {
    for (auto &i : PSets) {
      const auto &Pointer = i.first;
      PSet &PS = i.second;
      if (!PS.isValid())
        continue; // Nothing to invalidate

      if (PS.containsBase(O, order))
        SetPSet(Pointer, PSet::invalid(Reason), Reason.getLoc());
    }
  }
  void erasePointer(Variable P) { PSets.erase(P); }
  PSet GetPSet(Variable P);
  void SetPSet(Variable P, PSet PS, SourceLocation Loc);
  PSet derefPSet(PSet P, SourceLocation Loc);

  void diagPSet(Variable P, SourceLocation Loc) {
    auto i = PSets.find(P);
    if (i != PSets.end())
      Reporter->debugPset(Loc, P.getName(), i->second.str());
    else
      Reporter->debugPset(Loc, P.getName(), "(untracked)");
  }

  bool HandleClangAnalyzerPset(const CallExpr *CallE);

public:
  PSetsBuilder(const LifetimeReporterBase *Reporter, ASTContext &ASTCtxt,
               PSetsMap &PSets)
      : Reporter(Reporter), ASTCtxt(ASTCtxt), PSets(PSets) {}

  void EvalVarDecl(const VarDecl *VD) {
    const Expr *Initializer = VD->getInit();
    SourceLocation Loc = VD->getLocEnd();

    switch (classifyTypeCategory(VD->getType())) {
    case TypeCategory::Pointer: {
      PSet PS;
      if (Initializer)
        PS = EvalExprForPSet(
            Initializer, !VD->getType().getCanonicalType()->isPointerType());
      else if (Loc.isValid())
        PS = PSet::invalid(InvalidationReason::NotInitialized(Loc));

      SetPSet(VD, PS, Loc);
      break;
    }
    case TypeCategory::Owner: {
      SetPSet(VD, PSet::pointsToVariable(VD, false, 1), Loc);
    } // fallthrough
    default: {
      if (Initializer)
        EvalExpr(Initializer);
    }
    }
  }

  void VisitBlock(const CFGBlock &B);

  void UpdatePSetsFromCondition(const Stmt *S, bool Positive,
                                SourceLocation Loc);
};

// Manages lifetime information for the CFG of a FunctionDecl
PSet PSetsBuilder::GetPSet(Variable P) {
  auto i = PSets.find(P);
  if (i != PSets.end())
    return i->second;

  // Assumption: global Pointers have a pset of {static, null}
  if (P.hasGlobalStorage() || P.isMemberVariableOfEnclosingClass())
    return PSet::staticVar(P.mightBeNull());

  llvm::errs() << "PSetsBuilder::GetPSet: did not find pset for " << P.getName()
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
    else if (V.isCategoryPointer())
      RetPS.merge(GetPSet(V));
  }

  if (RetPS.containsNull())
    RetPS.appendNullReason(Loc);

  if (RetPS.containsInvalid())
    RetPS.appendInvalidReason(InvalidationReason::Dereferenced(Loc));

  return RetPS;
}

void PSetsBuilder::SetPSet(Variable P, PSet PS, SourceLocation Loc) {
  assert(P.getName() != "");
  if (!P.trackPset())
    return;

  // Assumption: global Pointers have a pset that is a subset of {static,
  // null}
  if (P.hasGlobalStorage() && !PS.isUnknown() &&
      !PS.isSubstitutableFor(PSet::staticVar(true)) && Reporter)
    Reporter->warnPsetOfGlobal(Loc, P.getName(), PS.str());

  auto i = PSets.find(P);
  if (i != PSets.end())
    i->second = std::move(PS);
  else
    PSets.emplace(P, PS);
}

void PSetsBuilder::CheckPSetValidity(const PSet &PS, SourceLocation Loc,
                                     bool flagNull) {
  if (!Reporter)
    return;

  if (PS.isUnknown()) {
    Reporter->diag(Loc, diag::warn_deref_unknown);
    return;
  }

  if (PS.containsInvalid()) {
    Reporter->warnDerefDangling(Loc, !PS.isInvalid());
    PS.explainWhyInvalid(*Reporter);
    return;
  }

  if (PS.containsNull()) {
    Reporter->warnDerefNull(Loc, !PS.isNull());
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
void PSetsBuilder::UpdatePSetsFromCondition(const Stmt *S, bool Positive,
                                            SourceLocation Loc) {
  const auto *E = dyn_cast_or_null<Expr>(S);
  if (!E)
    return;
  E = E->IgnoreParenImpCasts();
  if (const auto *UO = dyn_cast<UnaryOperator>(E)) {
    if (UO->getOpcode() != UO_LNot)
      return;
    E = UO->getSubExpr();
    UpdatePSetsFromCondition(E, !Positive, E->getLocStart());
    return;
  }
  if (const auto *BO = dyn_cast<BinaryOperator>(E)) {
    BinaryOperator::Opcode OC = BO->getOpcode();
    if (OC != BO_NE && OC != BO_EQ)
      return;
    // The p == null is the negative case.
    if (OC == BO_EQ)
      Positive = !Positive;
    const auto *LHS = BO->getLHS()->IgnoreParenImpCasts();
    const auto *RHS = BO->getRHS()->IgnoreParenImpCasts();
    Optional<Variable> LHSVar = refersTo(LHS);
    Optional<Variable> RHSVar = refersTo(RHS);
    bool LHSIsNull =
        LHS->isNullPointerConstant(
            ASTCtxt, Expr::NPC_ValueDependentIsNotNull) != Expr::NPCK_NotNull;
    bool RHSIsNull =
        RHS->isNullPointerConstant(
            ASTCtxt, Expr::NPC_ValueDependentIsNotNull) != Expr::NPCK_NotNull;
    if (LHSIsNull || (LHSVar && GetPSet(*LHSVar).isNull()))
      E = RHS;
    else if (RHSIsNull || (RHSVar && GetPSet(*RHSVar).isNull()))
      E = LHS;
    else
      return;
  }

  if (Optional<Variable> V = refersTo(E)) {
    if (V->trackPset()) {
      auto PS = GetPSet(*V);
      if (Positive)
        PS.removeNull();
      else
        PS = PSet::null(Loc);
      SetPSet(*V, PS, Loc);
    }
  }
}

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
                     .Case("__lifetime_type_category", 2)
                     .Case("__lifetime_type_category_arg", 3)
                     .Default(0);
  if (FuncNum == 0)
    return false;
  if (!Reporter)
    return true;

  auto Loc = CallE->getLocStart();
  switch (FuncNum) {
  case 1: {
    assert(CallE->getNumArgs() == 1 &&
           "clang_analyzer_pset takes one argument");

    // TODO: handle MemberExprs.
    const auto *DeclRef =
        dyn_cast<DeclRefExpr>(CallE->getArg(0)->IgnoreImpCasts());
    assert(DeclRef && "Argument to clang_analyzer_pset must be a DeclRefExpr");

    const auto *VD = dyn_cast<VarDecl>(DeclRef->getDecl());
    assert(VD && "Argument to clang_analyzer_pset must be a reference to "
                 "a VarDecl");

    diagPSet(VD, Loc);
    return true;
  }
  case 2: {
    auto Args = Callee->getTemplateSpecializationArgs();
    auto QType = Args->get(0).getAsType();
    TypeCategory TC = classifyTypeCategory(QType);
    Reporter->debugTypeCategory(Loc, TC);
    return true;
  }
  case 3: {
    auto QType = CallE->getArg(0)->getType();
    TypeCategory TC = classifyTypeCategory(QType);
    Reporter->debugTypeCategory(Loc, TC);
    return true;
  }
  default:
    llvm_unreachable("Unknown debug function.");
  }
}

// Update PSets in Builder through all CFGElements of this block
void PSetsBuilder::VisitBlock(const CFGBlock &B) {
  for (auto i = B.begin(); i != B.end(); ++i) {
    const CFGElement &E = *i;
    switch (E.getKind()) {
    case CFGElement::Statement: {
      const Stmt *S = E.castAs<CFGStmt>().getStmt();
      EvalStmt(S);

      // Kill all temporaries that vanish at the end of the full expression
      invalidateOwner(Variable::temporary(), 0,
                      InvalidationReason::TemporaryLeftScope(S->getLocEnd()));
      break;
    }
    case CFGElement::LifetimeEnds: {
      auto Leaver = E.castAs<CFGLifetimeEnds>();

      // Stop tracking Pointers that leave scope
      erasePointer(Leaver.getVarDecl());

      // Invalidate all pointers that track leaving Owners
      invalidateOwner(
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
}

void VisitBlock(PSetsMap &PSets, const CFGBlock &B,
                const LifetimeReporterBase *Reporter, ASTContext &ASTCtxt) {
  PSetsBuilder Builder(Reporter, ASTCtxt, PSets);
  Builder.VisitBlock(B);
}

void UpdatePSetsFromCondition(PSetsMap &PSets,
                              const LifetimeReporterBase *Reporter,
                              ASTContext &ASTCtxt, const Stmt *S, bool Positive,
                              SourceLocation Loc) {
  PSetsBuilder Builder(Reporter, ASTCtxt, PSets);
  Builder.UpdatePSetsFromCondition(S, Positive, Loc);
}

void EvalVarDecl(PSetsMap &PSets, const VarDecl *VD,
                 const LifetimeReporterBase *Reporter, ASTContext &ASTCtxt) {
  PSetsBuilder Builder(Reporter, ASTCtxt, PSets);
  Builder.EvalVarDecl(VD);
}

} // namespace lifetime
} // namespace clang