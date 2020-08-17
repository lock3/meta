//===--- SemaInject.cpp - Semantic Analysis for Injection -----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements semantic rules for the injection of declarations into
//  various declarative contexts.
//
//===----------------------------------------------------------------------===//

#include "TreeTransform.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTDiagnostic.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclVisitor.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/Type.h"
#include "clang/Sema/Initialization.h"
#include "clang/Sema/Template.h"
#include "clang/Sema/SemaInternal.h"

using namespace clang;

namespace clang {
  enum InjectedDefType : unsigned;
  struct InjectionInfo;
  class InjectionContext;
  class FunctionProtoType;
}

template<typename FromDeclType, typename ToDeclType, InjectedDefType DefType>
static void InjectPendingDefinitions(InjectionContext *Ctx,
                                     InjectionInfo *Injection);

template<typename DeclType, InjectedDefType DefType>
static void InjectPendingDefinitions(InjectionContext *Ctx,
                                     InjectionInfo *Injection);

namespace clang {

/// A compile-time value along with its type.
struct TypedValue {
  TypedValue(QualType T, const APValue& V) : Type(T), Value(V) { }

  QualType Type;
  APValue Value;
};

enum InjectedDefType : unsigned {
  InjectedDef_Field,
  InjectedDef_Method,
  InjectedDef_TemplatedMethod,
  InjectedDef_FriendFunction
};

/// Records information about a definition inside a fragment that must be
/// processed later. These are typically fields and methods.
struct InjectedDef {
  InjectedDef(const InjectedDefType& T, Decl *F, Decl *I) :
    Type(T), Fragment(F), Injected(I) { }

  InjectedDefType Type;

  /// The declaration within the fragment.
  Decl *Fragment;

  /// The injected declaration.
  Decl *Injected;
};

class InjectionCapture {
  enum {
   Legacy, Explicit
  } Kind;
  const ValueDecl *Decl;
  APValue Value;

public:
  InjectionCapture(const ValueDecl *Decl, APValue Value)
    : Kind(Legacy), Decl(Decl), Value(Value) { }
  InjectionCapture(APValue Value)
    : Kind(Explicit), Value(Value) { }

  bool isLegacy() const {
    return Kind == Legacy;
  }

  const ValueDecl *getDecl() const {
    assert(isLegacy());
    return Decl;
  }

  APValue getValue() const {
    return Value;
  }
};

using InjectionType = llvm::PointerUnion<Decl *, CXXBaseSpecifier *>;

struct InjectionInfo {
  InjectionInfo(InjectionType Injection, InjectionInfo *ParentInjection)
    : Injection(Injection), PointOfInjection(), Modifiers(),
      ParentInjection(ParentInjection), InitializingFragment(),
      CapturedValues() { }


  InjectionInfo *PrepareForPending() {
    // Reset the declaration modifiers. They're already been applied and
    // must not apply to nested declarations in a definition.
    Modifiers = ReflectionModifiers();

    return this;
  }

  bool hasPendingClassMemberData() const {
    if (!InjectedDefinitions.empty())
      return true;

    if (InjectedFieldData)
      return true;

    return false;
  }

  void ResetClassMemberData() {
    InjectedDefinitions.clear();
    InjectedFieldData = false;
  }

  /// The entity being Injected.
  InjectionType Injection;

  /// The source location of the triggering injector.
  SourceLocation PointOfInjection;

  /// The modifiers to apply to injection.
  ReflectionModifiers Modifiers;

  /// The optional parent injection, of this injection.
  InjectionInfo * const ParentInjection;

  /// The set of local declarations that have been transformed.
  /// These are declaration mappings that only make sense when paired
  /// with the injection, as they may be mapped to different declarations
  /// across different injections. Currently, this means injections
  /// which occur during fragment injection, as apposed to declaration
  /// cloning.
  llvm::DenseMap<Decl *, Decl *> TransformedLocalDecls;

  /// A mapping of fragment placeholders to their typed compile-time
  /// values. This is used by TreeTransformer to replace references with
  /// constant expressions.
  llvm::DenseMap<Decl *, TypedValue> PlaceholderSubsts;

  /// The initializing fragment for this fragment injection,
  /// and for any captured values.
  const CXXFragmentExpr *InitializingFragment;

  /// An index based mapping of captures to their values.
  llvm::SmallVector<APValue, 10> CapturedValues;

  /// True if we've injected a field. If we have, this injection context
  /// must be preserved until we've finished rebuilding all injected
  /// constructors.
  bool InjectedFieldData = false;

  /// A list of declarations whose definitions have not yet been
  /// injected. These are processed when a class receiving injections is
  /// completed.
  llvm::SmallVector<InjectedDef, 8> InjectedDefinitions;

  /// The template arguments to apply when injecting the injection.
  Sema::FragInstantiationArgTy TemplateArgs;

  /// The required relative declarations that should be part of the
  /// decl replacement mapping.
  llvm::SmallVector<Decl *, 8> RequiredRelatives;
};

/// An injection context. This is declared to establish a set of
/// substitutions during an injection.
class InjectionContext : public TreeTransform<InjectionContext> {
  using Base = TreeTransform<InjectionContext>;

  using InjectionType = llvm::PointerUnion<Decl *, CXXBaseSpecifier *, Stmt *>;
public:
  InjectionContext(Sema &SemaRef, Decl *Injectee)
    : Base(SemaRef), Injectee(Injectee) { }

  ~InjectionContext() {
    // Cleanup any allocated parameter injection arrays.
    for (auto *ParmVectorPtr : ParamInjectionCleanups) {
      delete ParmVectorPtr;
    }

    // Cleanup any allocated injection extras.
    for (auto *Injection : PendingInjections) {
      delete Injection;
    }
  }

  ASTContext &getContext() { return getSema().Context; }

  class RebuildOnlyContextRAII {
    InjectionContext &IC;
    DeclContext *OriginalRebuildOnlyContext;
    bool OriginalInstantiateTemplates;
  public:
    RebuildOnlyContextRAII(
        InjectionContext &IC, bool InstantiateTemplates = false)
      : IC(IC), OriginalRebuildOnlyContext(IC.RebuildOnlyContext),
        OriginalInstantiateTemplates(IC.InstantiateTemplates) {
      IC.RebuildOnlyContext = IC.getSema().CurContext;
      IC.InstantiateTemplates &= InstantiateTemplates;
    }
    ~RebuildOnlyContextRAII() {
      IC.InstantiateTemplates = OriginalInstantiateTemplates;
      IC.RebuildOnlyContext = OriginalRebuildOnlyContext;
    }
  };

  template<typename IT, typename F>
  void InitInjection(IT Injection, F Op) {
    InjectionInfo *ParentInjection = CurInjection;
    CurInjection = new InjectionInfo(Injection, ParentInjection);

    Op();

    // If we're injecting into a class and have pending definitions, attach
    // those to the class for subsequent analysis.
    if (!Injectee->isInvalidDecl()
        && CurInjection->hasPendingClassMemberData()) {
      assert(isa<CXXRecordDecl>(Injectee)
             && "All pending members should have been injected");
      PendingInjections.push_back(CurInjection->PrepareForPending());
    } else {
      delete CurInjection;
    }

    CurInjection = ParentInjection;
  }

  bool hasPendingInjections() const { return !PendingInjections.empty(); }

  void AddPendingDefinition(const InjectedDef &Def) {
    CurInjection->InjectedDefinitions.push_back(Def);
  }

  void InjectedFieldData() {
    CurInjection->InjectedFieldData = true;
  }

  template<typename F>
  void ForEachPendingInjection(F Op) {
    InjectionInfo *PreviousInjection = CurInjection;

    for (InjectionInfo *Injection : PendingInjections) {
      CurInjection = Injection;

      Op();
    }

    CurInjection = PreviousInjection;
  }

  int getLevelsSubstituted() {
    int Depth = 0;
    if (hasTemplateArgs()) {
      for (const auto &TemplateArgs : getTemplateArgs()) {
        Depth += TemplateArgs.getNumSubstitutedLevels();
      }
    }
    return Depth;
  }

  llvm::DenseMap<Decl *, Decl *> &GetDeclTransformMap() {
    if (isInjectingFragment()) {
      return CurInjection->TransformedLocalDecls;
    } else {
      return TransformedLocalDecls;
    }
  }

  void transformedLocalDecl(Decl *Old, Decl *New) {
    auto &TransformedDecls = GetDeclTransformMap();
    assert(TransformedDecls.count(Old) == 0 && "Overwriting substitution");
    TransformedDecls[Old] = New;
  }

  void transformedLocalDecl(Decl *Old, ArrayRef<Decl *> New) {
    // FIXME: Handle number of new decls.
    transformedLocalDecl(Old, New.front());
  }

  /// Adds a substitution from one declaration to another.
  void AddDeclSubstitution(const Decl *Old, Decl *New) {
    transformedLocalDecl(const_cast<Decl*>(Old), New);
  }

  /// Adds a substitution if it does not already exists.
  void MaybeAddDeclSubstitution(Decl *Old, Decl *New) {
    if (Decl *Replacement = GetDeclReplacement(Old)) {
      assert(Replacement == New && "Overwriting substitution");
      return;
    }
    AddDeclSubstitution(Old, New);
  }

  /// Adds substitutions for each placeholder in the fragment.
  /// The types and values are sourced from the fields of the reflection
  /// class and the captured values.
  void AddLegacyPlaceholderSubstitutions(
      const DeclContext *Fragment, const ArrayRef<InjectionCapture> &Captures) {
    assert(isa<CXXFragmentDecl>(Fragment) && "Context is not a fragment");

    auto PlaceIter = Fragment->decls_begin();
    auto PlaceIterEnd = Fragment->decls_end();
    auto CaptureIter = Captures.begin();
    auto CaptureIterEnd = Captures.end();

    while (PlaceIter != PlaceIterEnd && CaptureIter != CaptureIterEnd) {
      const InjectionCapture &IC = (*CaptureIter++);
      if (!IC.isLegacy())
        continue;

      Decl *Var = *PlaceIter++;

      QualType Ty = IC.getDecl()->getType();
      APValue Val = IC.getValue();

      CurInjection->PlaceholderSubsts.try_emplace(Var, Ty, Val);
    }
  }

  /// Adds the evaluated value for each capture mapped by itself
  /// index.
  void AddCapturedValues(const CXXFragmentExpr *InitializingFragment,
      const ArrayRef<InjectionCapture> &Captures) {
    assert(!CurInjection->InitializingFragment);
    CurInjection->InitializingFragment = InitializingFragment;

    for (const InjectionCapture &IC : Captures) {
      if (IC.isLegacy())
        continue;

      CurInjection->CapturedValues.emplace_back(IC.getValue());
    }
  }


  // Applies template arguments to the injection info for the current
  // executing fragment injection.
  void AddTemplateArgs(const Sema::FragInstantiationArgTy &TemplateArgs) {
    assert(isInjectingFragment());
    assert(CurInjection->TemplateArgs.empty());

    if (TemplateArgs.empty())
      return;

    CurInjection->TemplateArgs = TemplateArgs;
  }

  bool hasTemplateArgs() {
    assert(isInjectingFragment());
    return !CurInjection->TemplateArgs.empty();
  }

  const Sema::FragInstantiationArgTy &getTemplateArgs() {
    assert(hasTemplateArgs());
    return CurInjection->TemplateArgs;
  }

  // Adds this declaration as a required relative, a required relative
  // is one that should be remapped from a declaration clone.
  //
  // For instance, in the following example, `x` is a required
  // relative of `get_x`.
  //
  // class example {
  //   int x;
  //
  //   int get_x() {
  //     return x;
  //   }
  // };
  void AddRequiredRelative(Decl *D) {
    assert(!isInjectingFragment());
    CurInjection->RequiredRelatives.push_back(D);
  }

  void verifyHasAllRequiredRelatives() {
    SourceLocation POI = CurInjection->PointOfInjection;
    for (Decl *D : CurInjection->RequiredRelatives) {
      if (GetDeclReplacement(D))
        continue;

      SemaRef.Diag(POI, diag::err_injection_missing_required_relative)
        << cast<NamedDecl>(D) << cast<NamedDecl>(Injectee);
      SemaRef.Diag(D->getLocation(), diag::note_required_relative_declared)
        << cast<NamedDecl>(D);
    }
  }

  bool ShouldInjectInto(DeclContext *DC) const {
    // When not doing a declaration rebuild, this should always be true.
    //
    // However, when we're rebuilding, we check to see if we're attempting
    // to inject into the rebuild only context. If we are, we want
    // to block this injection, as we don't want any declarations added
    // to the rebuild only context, while it's set.
    return DC != RebuildOnlyContext;
  }

  Decl *GetDeclReplacement(Decl *D, llvm::DenseMap<Decl *, Decl *> &TransformedDecls) {
    auto Iter = TransformedDecls.find(D);
    if (Iter != TransformedDecls.end())
      return Iter->second;
    else
      return nullptr;
  }

  Decl *GetDeclReplacementFragment(Decl *D, InjectionInfo *II) {
    assert(isInjectingFragment());

    do {
      if (Decl *Replacement = GetDeclReplacement(D, II->TransformedLocalDecls))
        return Replacement;

      II = II->ParentInjection;
    } while (II);

    return nullptr;
  }

  /// Returns a replacement for D if a substitution has been registered or
  /// nullptr if no such replacement exists.
  Decl *GetDeclReplacement(Decl *D) {
    if (isInjectingFragment()) {
      return GetDeclReplacementFragment(D, CurInjection);
    } else {
      return GetDeclReplacement(D, TransformedLocalDecls);
    }
  }

  /// Returns a replacement expression if E refers to a placeholder.
  Expr *GetPlaceholderReplacement(DeclRefExpr *E) {
    auto &PlaceholderSubsts = CurInjection->PlaceholderSubsts;
    auto Iter = PlaceholderSubsts.find(E->getDecl());
    if (Iter != PlaceholderSubsts.end()) {
      // Build a new constant expression as the replacement. The source
      // expression is opaque since the actual declaration isn't part of
      // the output AST (but we might want it as context later -- makes
      // pretty printing more elegant).
      const TypedValue &TV = Iter->second;
      Expr *Opaque = new (getContext()) OpaqueValueExpr(
          E->getLocation(), TV.Type, VK_RValue, OK_Ordinary, E);
      auto *CE = ConstantExpr::Create(getContext(), Opaque, TV.Value);

      // Override dependence.
      //
      // By passing the DeclRefExpr onto OpaqueValueExpr we inherit
      // its dependence flags, and that is then inherited by the ConstantExpr.
      // Rather than causing conflict with upstream and changing
      // semantics of OpaqueValueExpr
      // (to allow us to traffic the DeclRefExpr for diagnostics),
      // override this behavior for the ConstantExpr.
      //
      // This is fine as we should only be using the ConstantExpr's value
      // rather than the expression which created it.

      CE->setDependence(ExprDependence::None);

      return CE;
    } else {
      return nullptr;
    }
  }

  ExprResult TransformCXXFragmentCaptureExpr(CXXFragmentCaptureExpr *E) {
    if (!isInjectingFragment())
      return Base::TransformCXXFragmentCaptureExpr(E);

    // Rebuild to create an updated capture expr with correct typing.
    ExprResult NewCapture = getDerived().RebuildCXXFragmentCaptureExpr(
        E->getBeginLoc(), CurInjection->InitializingFragment,
        E->getOffset(), E->getEndLoc());
    if (NewCapture.isInvalid())
      return ExprError();

    E = cast<CXXFragmentCaptureExpr>(NewCapture.get());

    // Apply the captured value via a ConstantExpr.
    APValue &CapturedVal = CurInjection->CapturedValues[E->getOffset()];
    return ConstantExpr::Create(getContext(), E, CapturedVal);
  }

  QualType GetRequiredType(const CXXRequiredTypeDecl *D) {
    auto Iter = RequiredTypes.find(D);
    if (Iter != RequiredTypes.end())
      return Iter->second;
    return QualType();
  }

  /// Returns true if D is within an injected fragment or cloned declaration.
  bool isInInjection(Decl *D);

  Decl *getDeclBeingInjected() const {
    return CurInjection->Injection.dyn_cast<Decl *>();
  }

  DeclContext *getInjectionDeclContext() const {
    if (Decl *ID = getDeclBeingInjected())
      return ID->getDeclContext();
    return nullptr;
  }

  /// Returns true if this context is injecting a fragment.
  bool isInjectingFragment() {
    return CurInjection->InitializingFragment;
  }

  /// Returns true if this context is injecting a fragment
  /// into a fragment.
  bool isRebuildOnly() {
    return RebuildOnlyContext;
  }

  void SetPointOfInjection(SourceLocation PointOfInjection) {
    CurInjection->PointOfInjection = PointOfInjection;
  }

  /// Sets the declaration modifiers.
  void SetModifiers(const ReflectionModifiers &Modifiers) {
    this->CurInjection->Modifiers = Modifiers;
  }

  ReflectionModifiers &GetModifiers() const {
    return this->CurInjection->Modifiers;
  }

  /// True if a rename is requested.
  bool hasRename() const { return GetModifiers().hasRename(); }

  DeclarationName applyRename() {
    ReflectionModifiers &Modifiers = GetModifiers();

    std::string NewName = Modifiers.getNewNameAsString();
    IdentifierInfo *II = &SemaRef.Context.Idents.get(NewName);

    // Reset the rename, so that it applies once, at the top level
    // of the injection (hopefully).
    //
    // FIXME: This is a sign of some fragility. We'd like the rename to
    // associate only with the fragment/decl we're replaying. This is
    // true of other modifiers also.
    Modifiers.setNewName(nullptr);

    return DeclarationName(II);
  }

  DeclarationNameInfo TransformDeclarationName(NamedDecl *ND) {
    DeclarationNameInfo DNI(ND->getDeclName(), ND->getLocation());
    return TransformDeclarationNameInfo(DNI);
  }

  DeclarationNameInfo TransformDeclarationNameInfo(DeclarationNameInfo DNI) {
    if (hasRename())
      DNI = DeclarationNameInfo(applyRename(), DNI.getLoc());

    return Base::TransformDeclarationNameInfo(DNI);
  }

  Decl *TransformDecl(SourceLocation Loc, Decl *D) {
    if (!D)
      return nullptr;

    // If we've EVER seen a replacement, then return that.
    if (Decl *Repl = GetDeclReplacement(D))
      return Repl;

    // If D is part of the injection, then we must have seen a previous
    // declaration. Otherwise, return nullptr and force a lookup or error.
    //
    // FIXME: This may not be valid for gotos and labels.
    if (isInInjection(D))
      return nullptr;

    if (!isInjectingFragment()) {
      // When copying existing declarations, if D is a member of the of the
      // injection's declaration context, then we want to re-map that so that the
      // result is a member of the injection. For example:
      //
      //    struct S {
      //      int x;
      //      int f() {
      //        return x; // Not dependent, bound
      //      }
      //    };
      //
      //    struct T { consteval { -> reflexpr(S::f); } };
      //
      // At the point that we inject S::f, the reference to x is not dependent,
      // and therefore not subject to two-phase lookup. However, we would expect
      // the reference to be to the T::x during injection.
      //
      // Note that this isn't necessary for fragments. We expect names to be
      // written dependently there and subject to the usual name resolution
      // rules.
      //
      // Defer the resolution to the caller so that the result can be
      // interpreted within the context of the expression, not here.
      //
      // If this condition is true, we're injecting a decl who's in the same
      // decl context as the declaration we're currently cloning.
      if (D->getDeclContext() == getInjectionDeclContext()) {
        AddRequiredRelative(D);
        return nullptr;
      }
    } else if (hasTemplateArgs() && InstantiateTemplates) {
      if (isa<CXXRecordDecl>(D)) {
        auto *Record = cast<CXXRecordDecl>(D);
        if (!Record->isDependentContext())
          return D;

        for (const auto &TemplateArgs : getTemplateArgs()) {
          Record = cast<CXXRecordDecl>(SemaRef.FindInstantiatedDecl(
              Loc, Record, TemplateArgs));
        }

        return Record;
      }
    }

    return D;
  }

  Decl *TransformDefinition(SourceLocation Loc, Decl *D) {
    // Rebuild the by injecting it. This will apply substitutions to the type
    // and initializer of the declaration.
    return InjectDecl(D);
  }

  ExprResult TransformDeclRefExpr(DeclRefExpr *E) {
    if (Expr *R = GetPlaceholderReplacement(E))
      return R;

    if (isa<NonTypeTemplateParmDecl>(E->getDecl())) {
      if (hasTemplateArgs() && InstantiateTemplates) {
        ExprResult Result = E;
        for (const auto &TemplateArgs : getTemplateArgs()) {
          if (Result.isInvalid())
            return Result;

          Result = getSema().SubstExpr(Result.get(), TemplateArgs);
        }
        return Result;
      }
    }

    return Base::TransformDeclRefExpr(E);
  }

  QualType MaybeIncreaseDepth(QualType QT) {
    // If we're not correcting template depth, or if our TemplateTypeParm was
    // successfully substituted, return the existing QualType.
    if (!IncreaseTemplateDepth || !isa<TemplateTypeParmType>(QT.getTypePtr()))
      return QT;

    const auto *T = cast<TemplateTypeParmType>(QT.getTypePtr());
    return getSema().Context.getTemplateTypeParmType(
        T->getDepth() + IncreaseTemplateDepth, T->getIndex(),
        T->isParameterPack(), T->getDecl());
  }

  QualType TransformTemplateTypeParmType(TypeLocBuilder &TLB,
                                         TemplateTypeParmTypeLoc TL) {
    if (hasTemplateArgs() && InstantiateTemplates) {
      // FIXME: Provide better source info
      TypeSourceInfo *TSI = getSema().SubstType(
          TL, *getTemplateArgs().begin(), SourceLocation(), DeclarationName());

      bool IsFirst = true;
      for (const auto &TemplateArgs : getTemplateArgs()) {
        if (IsFirst) {
          IsFirst = false;
          continue;
        }

        TSI = getSema().SubstType(
            TSI, TemplateArgs, SourceLocation(), DeclarationName());
      }

      QualType ResType = MaybeIncreaseDepth(TSI->getType());
      TLB.pushTypeSpec(ResType).setNameLoc(TL.getNameLoc());
      return ResType;
    }

    return Base::TransformTemplateTypeParmType(TLB, TL);
  }

  TemplateName TransformTemplateName(
      CXXScopeSpec &SS, TemplateName Name, SourceLocation NameLoc,
      QualType ObjectType = QualType(),
      NamedDecl *FirstQualifierInScope = nullptr,
      bool AllowInjectedClassName = false) {
    if (dyn_cast_or_null<TemplateTemplateParmDecl>(Name.getAsTemplateDecl())) {
      if (hasTemplateArgs() && InstantiateTemplates) {
        NestedNameSpecifierLoc &&SpecLoc = SS.getWithLocInContext(
                                                     getSema().getASTContext());

        TemplateName Result = Name;
        for (const auto &TemplateArgs : getTemplateArgs()) {
          Result = getSema().SubstTemplateName(
              SpecLoc, Result, NameLoc, TemplateArgs);
        }

        return Result;
      }
    }

    return Base::TransformTemplateName(
        SS, Name, NameLoc, ObjectType,
        FirstQualifierInScope, AllowInjectedClassName);
  }

  QualType RebuildCXXRequiredTypeType(CXXRequiredTypeDecl *D) {
    QualType RequiredType = GetRequiredType(D);

    // Case 1, we've found a replacement type, use it.
    if (!RequiredType.isNull()) {
      return RequiredType;
    }

    // Case 2, we haven't found a replacement type, but
    // we're only trying to rebuild currently, perform default
    // transform.
    if (isRebuildOnly()) {
      using Base = TreeTransform<InjectionContext>;
      return Base::RebuildCXXRequiredTypeType(D);
    }

    // Case 3, we haven't found a replacement type, and we
    // aren't rebuilding, an error should've been emitted during
    // injection of the RequirdeTypeDecl.
    return QualType();
  }

  ParmVarDecl *TransformFunctionTypeParam(
      ParmVarDecl *OldParm, int indexAdjustment,
      Optional<unsigned> NumExpansions, bool ExpectParameterPack) {
    ParmVarDecl *NewParm =
        TreeTransform<InjectionContext>::TransformFunctionTypeParam(
            OldParm, indexAdjustment, NumExpansions, ExpectParameterPack);

    if (NewParm)
      AddDeclSubstitution(OldParm, NewParm);

    return NewParm;
  }

  bool ExpandInjectedParameter(const CXXInjectedParmsInfo &Injected,
                               SmallVectorImpl<ParmVarDecl *> &Parms);

  bool ExpandInjectedParameters(
     ArrayRef<ParmVarDecl *> SourceParams, ArrayRef<ParmVarDecl *> &OutParams) {
#ifndef NDEBUG
    // Verify positions are what they will be restored to
    // by ContractInjectedParameters.
    {
      unsigned int counter = 0;

      for (ParmVarDecl *PVD : SourceParams) {
        assert(PVD->getFunctionScopeIndex() == counter++);
      }
    }
#endif

    // Create a new vector to hold the expanded params.
    auto *Params = new SmallVector<ParmVarDecl *, 8>();

    // Register the vector for cleanup during injection context teardown.
    ParamInjectionCleanups.push_back(Params);

    for (ParmVarDecl *Parm : SourceParams) {
      if (const CXXInjectedParmsInfo *InjectedInfo = Parm->InjectedParmsInfo) {
        SmallVector<ParmVarDecl *, 4> ExpandedParms;
        if (ExpandInjectedParameter(*InjectedInfo, ExpandedParms))
          return true;

        // Add the new Params.
        Params->append(ExpandedParms.begin(), ExpandedParms.end());

        // Add the substitution.
        InjectedParms[Parm] = ExpandedParms;
        continue;
      }

      Params->push_back(Parm);
    }

    // Correct positional information of the injected parameters.
    // This will be reverted by ContractInjectedParameters to ensure
    // we have not left a lasting effect on the source parameters.
    unsigned int counter = 0;
    for (ParmVarDecl *PVD : *Params) {
      PVD->setScopeInfo(PVD->getFunctionScopeDepth(), counter++);
    }

    OutParams = *Params;
    return false;
  }

  bool ContractInjectedParameters(ArrayRef<ParmVarDecl *> SourceParams) {
    unsigned int counter = 0;
    for (ParmVarDecl *PVD : SourceParams) {
      PVD->setScopeInfo(PVD->getFunctionScopeDepth(), counter++);
    }
    return false;
  }

  bool InjectDeclarator(DeclaratorDecl *D, DeclarationNameInfo &DNI,
                        TypeSourceInfo *&TSI);
  bool InjectMemberDeclarator(DeclaratorDecl *D, DeclarationNameInfo &DNI,
                              TypeSourceInfo *&TSI, CXXRecordDecl *&Owner);
  bool InjectBaseSpecifier(CXXBaseSpecifier *BS);

  void UpdateFunctionParms(FunctionDecl* Old, FunctionDecl* New);

  Decl *InjectNamespaceDecl(NamespaceDecl *D);
  Decl *InjectTypedefNameDecl(TypedefNameDecl *D);
  Decl *InjectFunctionDecl(FunctionDecl *D);
  Decl *InjectVarDecl(VarDecl *D);
  Decl *InjectCXXRecordDecl(CXXRecordDecl *D);
  Decl *InjectStaticDataMemberDecl(FieldDecl *D);
  Decl *InjectFieldDecl(FieldDecl *D);
  template<typename F>
  Decl *InjectCXXMethodDecl(CXXMethodDecl *D, F FinishBody);
  Decl *InjectCXXMethodDecl(CXXMethodDecl *D);
  Decl *InjectDeclImpl(Decl *D);
  Decl *InjectDecl(Decl *D);
  Decl *RebuildInjectDecl(Decl *D);
  Decl *InjectAccessSpecDecl(AccessSpecDecl *D);
  Decl *InjectFriendDecl(FriendDecl *D);
  Decl *InjectCXXMetaprogramDecl(CXXMetaprogramDecl *D);
  Decl *InjectCXXInjectionDecl(CXXInjectionDecl *D);

  TemplateParameterList *InjectTemplateParms(TemplateParameterList *Old);
  Decl *InjectClassTemplateDecl(ClassTemplateDecl *D);
  Decl *InjectClassTemplateSpecializationDecl(ClassTemplateSpecializationDecl *D);
  Decl *CreatePartiallySubstPattern(FunctionTemplateDecl *D);
  Decl *InjectFunctionTemplateDecl(FunctionTemplateDecl *D);
  Decl *InjectTemplateTypeParmDecl(TemplateTypeParmDecl *D);
  Decl *InjectNonTypeTemplateParmDecl(NonTypeTemplateParmDecl *D);
  Decl *InjectTemplateTemplateParmDecl(TemplateTemplateParmDecl *D);
  Decl *InjectStaticAssertDecl(StaticAssertDecl *D);
  Decl *InjectEnumDecl(EnumDecl *D);
  Decl *InjectEnumConstantDecl(EnumConstantDecl *D);
  Decl *InjectCXXRequiredTypeDecl(CXXRequiredTypeDecl *D);
  Decl *InjectCXXRequiredDeclaratorDecl(CXXRequiredDeclaratorDecl *D);

  Stmt *InjectStmtImpl(Stmt *S);
  Stmt *InjectStmt(Stmt *S);
  Stmt *InjectDeclStmt(DeclStmt *S);

  // Members

  /// A mapping of injected parameters to their corresponding
  /// expansions.
  llvm::DenseMap<ParmVarDecl *, SmallVector<ParmVarDecl *, 4>> InjectedParms;

  /// A mapping of required typenames to their corresponding declared types.
  llvm::DenseMap<CXXRequiredTypeDecl *, QualType> RequiredTypes;

  /// A mapping of requires declarators to their corresponding declarators.
  llvm::DenseMap<DeclaratorDecl *, DeclaratorDecl *> RequiredDecls;

  /// A list of expanded parameter injections to be cleaned up.
  llvm::SmallVector<SmallVector<ParmVarDecl *, 8> *, 4> ParamInjectionCleanups;

  SmallVectorImpl<ParmVarDecl *> *FindInjectedParms(ParmVarDecl *P) {
    auto Iter = InjectedParms.find(P);
    if (Iter == InjectedParms.end())
      return nullptr;
    return &Iter->second;
  }

  /// True if we've injected a field, or a declaration with
  /// an incomplete injection.
  bool ContainsClassData = false;

  /// Set by RebuildOnlyContextRAII to block injection.
  /// This variable is used to create a DeclContext which
  /// declarations should not be added to, allowing
  /// the rebuilding of declarations via injection.
  DeclContext *RebuildOnlyContext = nullptr;

  bool InstantiateTemplates = true;

  int IncreaseTemplateDepth = 0;

  /// The context into which the fragment is injected
  Decl *Injectee;

  /// The current injection being injected.
  InjectionInfo *CurInjection = nullptr;

  /// The pending class member injections.
  llvm::SmallVector<InjectionInfo *, 8> PendingInjections;

  /// Whether we're injecting pending definitions.
  bool InjectingPendingDefinitions = false;

  /// If this is a local block fragment, we will inject it into
  /// a statement rather than a context.
  Stmt *InjecteeStmt;

  /// A container that holds the injected stmts we will eventually
  /// build a new CompoundStmt out of.
  llvm::SmallVector<Stmt *, 8> InjectedStmts;

  /// The declarations which have been injected as "artifacts of injection"
  /// i.e. these declarations will be used by the caller into
  /// injection to append.
  ///
  /// This is currently only used for enum injection,
  /// as modifying the enclosing enum from inside of the injection
  /// mechanism is not viable.
  ///
  /// As a FIXME, we should look for a more unified approach.
  llvm::SmallVector<Decl *, 32> InjectedDecls;
};

bool InjectionContext::isInInjection(Decl *D) {
  // If this is actually a fragment, then we can check in the usual way.
  if (isInjectingFragment())
    return D->isInFragment();

  // Otherwise, we're cloning a declaration, (not a fragment) but we need
  // to ensure that any any declarations within that are injected.

  Decl *InjectionAsDecl = getDeclBeingInjected();
  if (!InjectionAsDecl)
    return false;

  // If D is injection source, then it must be injected.
  if (D == InjectionAsDecl)
    return true;

  // If the injection is not a DC, then D cannot be in the injection because
  // it could not have been declared within (e.g., if the injection is a
  // variable).
  DeclContext *InjectionAsDC = dyn_cast<DeclContext>(InjectionAsDecl);
  if (!InjectionAsDC)
    return false;

  // Otherwise, work outwards to see if D is in the Outermost context
  // of the injection.

  DeclContext *InjecteeAsDC = Decl::castToDeclContext(Injectee);
  DeclContext *DC = D->getDeclContext();
  while (DC) {
    // We're inside of the injection, as the DC is the source injection.
    if (DC == InjectionAsDC)
      return true;
    // We're outside of the injection, as the DC is the thing we're
    // injecting into.
    if (DC == InjecteeAsDC)
      return false;
    DC = DC->getParent();
  }
  return false;
}

static ParmVarDecl *GetReflectedPVD(const Reflection &R) {
  if (!R.isDeclaration())
    return nullptr;

  Decl *ReflectD = const_cast<Decl *>(R.getAsDeclaration());
  return dyn_cast<ParmVarDecl>(ReflectD);
}

bool InjectionContext::ExpandInjectedParameter(
                                        const CXXInjectedParmsInfo &Injected,
                                        SmallVectorImpl<ParmVarDecl *> &Parms) {
  EnterExpressionEvaluationContext EvalContext(
      SemaRef, Sema::ExpressionEvaluationContext::ConstantEvaluated);

  ExprResult TransformedOperand = getDerived().TransformExpr(
                                                            Injected.Operand);
  if (TransformedOperand.isInvalid())
    return true;

  Expr *Operand = TransformedOperand.get();
  Sema::ExpansionContextBuilder CtxBldr(SemaRef, SemaRef.getCurScope(),
                                        Operand);
  if (CtxBldr.BuildCalls()) {
    // TODO: Diag << failed to build calls
    return true;
  }

  // Traverse the range now and add the exprs to the vector
  Sema::RangeTraverser Traverser(SemaRef, CtxBldr.getKind(),
                                 CtxBldr.getRangeBeginCall(),
                                 CtxBldr.getRangeEndCall());

  while (!Traverser) {
    Expr *CurExpr = *Traverser;

    Reflection R;
    if (EvaluateReflection(SemaRef, CurExpr, R))
      return true;

    if (R.isInvalid()) {
      DiagnoseInvalidReflection(SemaRef, CurExpr, R);
      return true;
    }

    ParmVarDecl *ReflectedPVD = GetReflectedPVD(R);
    if (!ReflectedPVD) {
      SourceLocation &&CurExprLoc = CurExpr->getExprLoc();
      SemaRef.Diag(CurExprLoc, diag::err_reflection_not_parm_var_decl);
      return true;
    }

    Parms.push_back(ReflectedPVD);
    ++Traverser;
  }

  return false;
}

// Inject the name and the type of a declarator declaration. Sets the
// declaration name info, type, and owner. Returns true if the declarator
// is invalid.
//
// FIXME: If the declarator has a nested names specifier, rebuild that
// also. That potentially modifies the owner of the declaration
bool InjectionContext::InjectDeclarator(DeclaratorDecl *D,
                                        DeclarationNameInfo &DNI,
                                        TypeSourceInfo *&TSI) {
  bool Invalid = false;

  // Rebuild the name.
  DNI = TransformDeclarationName(D);
  if (D->getDeclName().isEmpty() != DNI.getName().isEmpty()) {
    DNI = DeclarationNameInfo(D->getDeclName(), D->getLocation());
    Invalid = true;
  }

  // Rebuild the type.
  TSI = TransformType(D->getTypeSourceInfo());
  if (!TSI) {
    TSI = D->getTypeSourceInfo();
    Invalid = true;
  }

  return Invalid;
}

// Inject the name and the type of a declarator declaration. Sets the
// declaration name info, type, and owner. Returns true if the declarator
// is invalid.
bool InjectionContext::InjectMemberDeclarator(DeclaratorDecl *D,
                                              DeclarationNameInfo &DNI,
                                              TypeSourceInfo *&TSI,
                                              CXXRecordDecl *&Owner) {
  bool Invalid = InjectDeclarator(D, DNI, TSI);

  Owner = cast<CXXRecordDecl>(getSema().CurContext);
  return Invalid;
}

static AccessSpecifier Transform(AccessModifier Modifier) {
  switch(Modifier) {
  case AccessModifier::Public:
    return AS_public;
  case AccessModifier::Protected:
    return AS_protected;
  case AccessModifier::Private:
    return AS_private;
  default:
    llvm_unreachable("Invalid access modifier transformation");
  }
}

template<typename NewType, typename OldType>
static void ApplyAccess(ReflectionModifiers Modifiers,
                        NewType* Decl, OldType* OriginalDecl) {
  if (Modifiers.modifyAccess()) {
    AccessModifier Modifier = Modifiers.getAccessModifier();

    if (Modifier == AccessModifier::Default) {
      TagDecl *TD = cast<TagDecl>(Decl->getDeclContext());
      Decl->setAccess(TD->getDefaultAccessSpecifier());
      return;
    }

    Decl->setAccess(Transform(Modifier));
    return;
  }

  Decl->setAccess(OriginalDecl->getAccess());
}

template<>
void ApplyAccess<CXXBaseSpecifier, CXXBaseSpecifier>(
    ReflectionModifiers Modifiers,
    CXXBaseSpecifier* Base, CXXBaseSpecifier* OriginalBase) {
  if (Modifiers.modifyAccess()) {
    AccessModifier Modifier = Modifiers.getAccessModifier();

    if (Modifier == AccessModifier::Default) {
      Base->setAccessSpecifier(AS_none);
      return;
    }

    Base->setAccessSpecifier(Transform(Modifier));
    return;
  }

  Base->setAccessSpecifier(OriginalBase->getAccessSpecifier());
}

bool InjectionContext::InjectBaseSpecifier(CXXBaseSpecifier *BS) {
  CXXRecordDecl *Owner = cast<CXXRecordDecl>(getSema().CurContext);

  SmallVector<CXXBaseSpecifier *, 4> Bases;
  for (CXXBaseSpecifier &Base : Owner->bases())
    Bases.push_back(&Base);
  for (CXXBaseSpecifier &Base : Owner->vbases())
    Bases.push_back(&Base);

  CXXBaseSpecifier *NewBase = TransformCXXBaseSpecifier(Owner, BS);
  if (!NewBase)
    return true;

  ApplyAccess(GetModifiers(), NewBase, BS);
  Bases.push_back(NewBase);

  if (getSema().AttachBaseSpecifiers(Owner, Bases))
    return true;

  return false;
}

static void UpdateFunctionParm(InjectionContext &Ctx, FunctionDecl *Function,
                               ParmVarDecl *OldParm, ParmVarDecl *NewParm) {
  NewParm->setOwningFunction(Function);

  ExprResult InitExpr = Ctx.TransformExpr(OldParm->getInit());
  NewParm->setInit(InitExpr.get());
}

void InjectionContext::UpdateFunctionParms(FunctionDecl* Old,
                                           FunctionDecl* New) {
  // Make sure the parameters are actually bound to the function.
  TypeSourceInfo *TSI = New->getTypeSourceInfo();
  FunctionProtoTypeLoc TL = TSI->getTypeLoc().castAs<FunctionProtoTypeLoc>();
  New->setParams(TL.getParams());

  // Update the parameters their owning functions and register substitutions
  // as needed. Note that we automatically register substitutions for injected
  // parameters.
  unsigned OldIndex = 0;
  unsigned NewIndex = 0;
  auto OldParms = Old->parameters();
  auto NewParms = New->parameters();

  // Visit all old params
  while (OldIndex < OldParms.size()) {
    ParmVarDecl *OldParm = OldParms[OldIndex++];
    if (auto *Injected = FindInjectedParms(OldParm)) {
      for (unsigned I = 0; I < Injected->size(); ++I) {
        ParmVarDecl *NewParm = NewParms[NewIndex++];
        UpdateFunctionParm(*this, New, OldParm, NewParm);
      }

      // This should not be replaced, as there's no 1-to-1 mapping.
      assert(GetDeclReplacement(OldParm) == nullptr);
    } else {
      ParmVarDecl *NewParm = NewParms[NewIndex++];
      UpdateFunctionParm(*this, New, OldParm, NewParm);

      // This should always be invalid,
      // or match the replacement by TransformFunctionTypeParam.
      assert(OldParm->InjectedParmsInfo ||
             GetDeclReplacement(OldParm) == NewParm);
    }
  }

  // Verify all old and new params have been visited
  assert(OldIndex == OldParms.size() && NewIndex == NewParms.size());
}

Decl* InjectionContext::InjectNamespaceDecl(NamespaceDecl *D) {
  DeclContext *Owner = getSema().CurContext;

  SourceLocation &&NamespaceLoc = D->getBeginLoc();
  SourceLocation &&Loc = D->getLocation();

  bool IsInline = D->isInline() || GetModifiers().addInline();
  bool IsInvalid = false;
  bool IsStd = false;
  bool AddToKnown = false;
  NamespaceDecl *PrevNS = nullptr;
  SemaRef.CheckNamespaceDeclaration(
      D->getIdentifier(), NamespaceLoc, Loc,
      IsInline, IsInvalid, IsStd, AddToKnown, PrevNS);

  // Build the namespace.
  NamespaceDecl *Ns = NamespaceDecl::Create(
      getContext(), Owner, IsInline, NamespaceLoc,
      Loc, D->getIdentifier(), PrevNS);
  AddDeclSubstitution(D, Ns);

  Owner->addDecl(Ns);

  // Inject the namespace members.
  Sema::ContextRAII NsCtx(getSema(), Ns);
  for (Decl *OldMember : D->decls()) {
    Decl *NewMember = InjectDecl(OldMember);
    if (!NewMember || NewMember->isInvalidDecl())
      Ns->setInvalidDecl(true);
  }

  return Ns;
}

Decl* InjectionContext::InjectTypedefNameDecl(TypedefNameDecl *D) {
  bool Invalid = false;

  DeclContext *Owner = getSema().CurContext;

  // Transform the type. If this fails, just retain the original, but
  // invalidate the declaration later.
  TypeSourceInfo *TSI = TransformType(D->getTypeSourceInfo());
  if (!TSI) {
    TSI = D->getTypeSourceInfo();
    Invalid = true;
  }

  // Create the new typedef
  TypedefNameDecl *Typedef;
  if (isa<TypeAliasDecl>(D))
    Typedef = TypeAliasDecl::Create(
        getContext(), Owner, D->getBeginLoc(), D->getLocation(),
        D->getIdentifier(), TSI);
  else
    Typedef = TypedefDecl::Create(
        getContext(), Owner, D->getBeginLoc(), D->getLocation(),
        D->getIdentifier(), TSI);
  AddDeclSubstitution(D, Typedef);

  ApplyAccess(GetModifiers(), Typedef, D);
  Typedef->setInvalidDecl(Invalid);
  Owner->addDecl(Typedef);

  return Typedef;
}

static bool InjectVariableInitializer(InjectionContext &Ctx,
                                      VarDecl *Old,
                                      VarDecl *New) {
  if (Old->getInit()) {
    if (New->isStaticDataMember() && !Old->isOutOfLine())
      Ctx.getSema().PushExpressionEvaluationContext(
          Sema::ExpressionEvaluationContext::ConstantEvaluated, Old);
    else
      Ctx.getSema().PushExpressionEvaluationContext(
          Sema::ExpressionEvaluationContext::PotentiallyEvaluated, Old);

    // Instantiate the initializer.
    ExprResult Init;
    {
      Sema::ContextRAII SwitchContext(Ctx.getSema(), New->getDeclContext());
      bool DirectInit = (Old->getInitStyle() == VarDecl::CallInit);
      Init = Ctx.TransformInitializer(Old->getInit(), DirectInit);
    }

    if (!Init.isInvalid()) {
      Expr *InitExpr = Init.get();
      if (New->hasAttr<DLLImportAttr>() &&
          (!InitExpr ||
           !InitExpr->isConstantInitializer(Ctx.getContext(), false))) {
        // Do not dynamically initialize dllimport variables.
      } else if (InitExpr) {
        Ctx.getSema().AddInitializerToDecl(New, InitExpr, Old->isDirectInit());
      } else {
        Ctx.getSema().ActOnUninitializedDecl(New);
      }
    } else {
      New->setInvalidDecl();
    }

    Ctx.getSema().PopExpressionEvaluationContext();
  } else {
    if (New->isStaticDataMember()) {
      if (!New->isOutOfLine())
        return New;

      // If the declaration inside the class had an initializer, don't add
      // another one to the out-of-line definition.
      if (Old->getFirstDecl()->hasInit())
        return New;
    }

    // We'll add an initializer to a for-range declaration later.
    if (New->isCXXForRangeDecl())
      return New;

    Ctx.getSema().ActOnUninitializedDecl(New);
  }

  return New;
}

static void CheckInjectedFunctionDecl(Sema &SemaRef, FunctionDecl *FD,
                                      DeclContext *Owner) {
  // FIXME: Is this right?
  LookupResult Previous(
    SemaRef, FD->getDeclName(), SourceLocation(),
    Sema::LookupOrdinaryName, SemaRef.forRedeclarationInCurContext());
  SemaRef.LookupQualifiedName(Previous, Owner);

  SemaRef.CheckFunctionDeclaration(/*Scope=*/nullptr, FD, Previous,
                                   /*IsMemberSpecialization=*/false);
}

static void InjectFunctionDefinition(InjectionContext *Ctx,
                                     FunctionDecl *OldFunction,
                                     FunctionDecl *NewFunction) {
  Sema &S = Ctx->getSema();

  S.ActOnStartOfFunctionDef(nullptr, NewFunction);

  Sema::SynthesizedFunctionScope Scope(S, NewFunction);
  Sema::ContextRAII FnCtx(S, NewFunction);

  StmtResult NewBody;
  if (Stmt *OldBody = OldFunction->getBody()) {
    NewBody = Ctx->TransformStmt(OldBody);
    if (NewBody.isInvalid())
      NewFunction->setInvalidDecl();
  }

  S.ActOnFinishFunctionBody(NewFunction, NewBody.get(),
                            /*IsInstantiation=*/true);
}

static ConstexprSpecKind Transform(ConstexprModifier Modifier) {
  switch(Modifier) {
  case ConstexprModifier::Constexpr:
    return CSK_constexpr;
  case ConstexprModifier::Consteval:
    return CSK_consteval;
  case ConstexprModifier::Constinit:
    return CSK_constinit;
  default:
    llvm_unreachable("Invalid constexpr modifier transformation");
  }
}

Decl *InjectionContext::InjectFunctionDecl(FunctionDecl *D) {
  DeclContext *Owner = getSema().CurContext;

  DeclarationNameInfo DNI;
  TypeSourceInfo* TSI;
  bool Invalid = InjectDeclarator(D, DNI, TSI);

  FunctionDecl* Fn = FunctionDecl::Create(
      getContext(), Owner, D->getLocation(), DNI, TSI->getType(), TSI,
      D->getStorageClass(), D->isInlineSpecified(), D->hasWrittenPrototype(),
      D->getConstexprKind(), D->getTrailingRequiresClause());
  AddDeclSubstitution(D, Fn);
  UpdateFunctionParms(D, Fn);

  // Update the constexpr specifier.
  if (GetModifiers().modifyConstexpr()) {
    Fn->setType(Fn->getType().withConst());

    ConstexprModifier Modifier = GetModifiers().getConstexprModifier();
    if (Modifier == ConstexprModifier::Constinit) {
      SemaRef.Diag(Fn->getLocation(), diag::err_modify_constinit_function);
      Fn->setInvalidDecl(true);
    } else {
      Fn->setConstexprKind(Transform(Modifier));
    }
  } else {
    Fn->setConstexprKind(D->getConstexprKind());
  }

  // Set properties.
  if (D->isInlineSpecified()) {
    Fn->setInlineSpecified();
  } else if (D->isInlined() || GetModifiers().addInline()) {
    Fn->setImplicitlyInline();
  }

  Fn->setInvalidDecl(Fn->isInvalidDecl() || Invalid);
  if (D->getFriendObjectKind() != Decl::FOK_None)
    Fn->setObjectOfFriendDecl();

  // Don't register the declaration if we're merely attempting to transform
  // this function.
  if (ShouldInjectInto(Owner)) {
    CheckInjectedFunctionDecl(getSema(), Fn, Owner);
    Owner->addDecl(Fn);
  }

  // If the function has a defined body, that it owns, inject that also.
  //
  // Note that namespace-scope function definitions are never deferred.
  // Also, function decls never appear in class scope (we hope),
  // so we shouldn't be doing this too early.
  if (D->isThisDeclarationADefinition()) {
    bool IsFriend = D->getFriendObjectKind() != Decl::FOK_None;
    if (IsFriend) {
      AddPendingDefinition(InjectedDef(InjectedDef_FriendFunction, D, Fn));
    } else {
      InjectFunctionDefinition(this, D, Fn);
    }
  }

  return Fn;
}

static void CheckInjectedVarDecl(Sema &SemaRef, VarDecl *VD,
                                 DeclContext *Owner) {
  // FIXME: Is this right?
  LookupResult Previous(
    SemaRef, VD->getDeclName(), VD->getLocation(),
    Sema::LookupOrdinaryName, SemaRef.forRedeclarationInCurContext());
  SemaRef.LookupQualifiedName(Previous, Owner);

  SemaRef.CheckVariableDeclaration(VD, Previous);

  // FIXME: Duplicates part of ActOnVariableDeclarator
  if (auto *RD = dyn_cast<CXXRecordDecl>(Owner)) {
    const DeclContext *ImmediateParent = RD->getParent();
    if (ImmediateParent && ImmediateParent->isFragment())
      return;

    if (RD->getDeclName())
      return;

    SemaRef.Diag(VD->getLocation(),
                 diag::err_static_data_member_not_allowed_in_anon_struct)
        << VD->getDeclName() << RD->getTagKind();
  }
}

Decl *InjectionContext::InjectVarDecl(VarDecl *D) {
  DeclContext *Owner = getSema().CurContext;

  DeclarationNameInfo DNI;
  TypeSourceInfo *TSI;
  bool Invalid = InjectDeclarator(D, DNI, TSI);

  VarDecl *Var = VarDecl::Create(
      getContext(), Owner, D->getInnerLocStart(), DNI.getLoc(), DNI.getName(),
      TSI->getType(), TSI, D->getStorageClass());
  AddDeclSubstitution(D, Var);

  if (D->isNRVOVariable()) {
    QualType ReturnType = cast<FunctionDecl>(Owner)->getReturnType();
    if (getSema().isCopyElisionCandidate(ReturnType, Var, Sema::CES_Strict))
      Var->setNRVOVariable(true);
  }

  Var->setImplicit(D->isImplicit());
  Var->setInvalidDecl(Invalid);

  // If we are instantiating a local extern declaration, the
  // instantiation belongs lexically to the containing function.
  // If we are instantiating a static data member defined
  // out-of-line, the instantiation will have the same lexical
  // context (which will be a namespace scope) as the template.
  if (D->isLocalExternDecl()) {
    Var->setLocalExternDecl();
    Var->setLexicalDeclContext(Owner);
  } else if (D->isOutOfLine()) {
    Var->setLexicalDeclContext(D->getLexicalDeclContext());
  }
  Var->setTSCSpec(D->getTSCSpec());
  Var->setInitStyle(D->getInitStyle());
  Var->setCXXForRangeDecl(D->isCXXForRangeDecl());

  if (GetModifiers().modifyConstexpr()) {
    Var->setType(Var->getType().withConst());
    switch (GetModifiers().getConstexprModifier()) {
    case ConstexprModifier::Constexpr:
      Var->setConstexpr(true);
      break;
    case ConstexprModifier::Consteval:
      SemaRef.Diag(Var->getLocation(), diag::err_modify_consteval_variable);
      Var->setInvalidDecl(true);
      break;
    case ConstexprModifier::Constinit:
      Var->addAttr(ConstInitAttr::Create(
          SemaRef.Context, Var->getLocation(),
          AttributeCommonInfo::AS_Keyword, ConstInitAttr::Keyword_constinit));
      break;
    default:
      llvm_unreachable("invalid constexpr modifier transformation");
    }
  } else {
    Var->setConstexpr(D->isConstexpr());
  }

  Var->setInitCapture(D->isInitCapture());
  Var->setPreviousDeclInSameBlockScope(D->isPreviousDeclInSameBlockScope());
  Var->setAccess(D->getAccess());

  if (!D->isStaticDataMember()) {
    if (D->isUsed(false))
      Var->setIsUsed();
    Var->setReferenced(D->isReferenced());
  }

  // Don't register the declaration if we're merely attempting to transform
  // this variable.
  if (ShouldInjectInto(Owner)) {
    CheckInjectedVarDecl(SemaRef, Var, Owner);
    Owner->addDecl(Var);
  }

  // FIXME: Instantiate attributes.

  // Forward the mangling number from the template to the instantiated decl.
  getContext().setManglingNumber(
      Var, getContext().getManglingNumber(D));
  getContext().setStaticLocalNumber(
      Var, getContext().getStaticLocalNumber(D));

  if (D->isInlineSpecified())
    Var->setInlineSpecified();
  else if (D->isInline() || GetModifiers().addInline())
    Var->setImplicitlyInline();

  InjectVariableInitializer(*this, D, Var);

  return Var;
}

/// Injects the base specifier Base into Class.
static bool InjectBaseSpecifiers(InjectionContext &Ctx,
                                 CXXRecordDecl *OldClass,
                                 CXXRecordDecl *NewClass) {
  bool Invalid = false;
  SmallVector<CXXBaseSpecifier*, 4> Bases;
  for (const CXXBaseSpecifier &OldBase : OldClass->bases()) {
    CXXBaseSpecifier *NewBase = Ctx.TransformCXXBaseSpecifier(NewClass,
                                                              &OldBase);
    if (!NewBase) {
      Invalid = true;
      continue;
    }

    Bases.push_back(NewBase);
  }

  if (!Invalid && Ctx.getSema().AttachBaseSpecifiers(NewClass, Bases))
    Invalid = true;

  // Invalidate the class if necessary.
  NewClass->setInvalidDecl(Invalid);

  return Invalid;
}

static bool InjectClassMembers(InjectionContext &Ctx,
                               CXXRecordDecl *OldClass,
                               CXXRecordDecl *NewClass) {
  for (Decl *OldMember : OldClass->decls()) {
    // Don't transform invalid declarations.
    if (OldMember->isInvalidDecl())
      continue;

    // Don't transform non-members appearing in a class.
    //
    // FIXME: What does it mean to inject friends?
    if (OldMember->getDeclContext() != OldClass)
      continue;

    Decl *NewMember = Ctx.InjectDecl(OldMember);
    if (!NewMember)
      NewClass->setInvalidDecl();
  }
  return NewClass->isInvalidDecl();
}

static bool InjectClassDefinition(InjectionContext &Ctx,
                                  CXXRecordDecl *OldClass,
                                  CXXRecordDecl *NewClass) {
  Sema::ContextRAII SwitchContext(Ctx.getSema(), NewClass);
  Ctx.getSema().StartDefinition(NewClass);
  InjectBaseSpecifiers(Ctx, OldClass, NewClass);
  InjectClassMembers(Ctx, OldClass, NewClass);
  Ctx.getSema().CompleteDefinition(NewClass);
  return NewClass->isInvalidDecl();
}

static NamedDecl *GetPreviousTagDecl(Sema &SemaRef, const DeclarationNameInfo &DNI,
                                     DeclContext *Owner) {
  LookupResult Previous(SemaRef, DNI, Sema::LookupTagName,
                        SemaRef.forRedeclarationInCurContext());
  SemaRef.LookupQualifiedName(Previous, Owner);

  return Previous.empty() ? nullptr : Previous.getFoundDecl();
}

template<typename T>
static void
CheckInjectedTagDecl(Sema &SemaRef, const DeclarationNameInfo &DNI,
                     DeclContext *Owner, T *D,
                     NamedDecl *&PrevDecl, bool &Invalid) {
  if (!DNI.getName())
    Invalid = true;

  PrevDecl = GetPreviousTagDecl(SemaRef, DNI, Owner);
  if (TagDecl *PrevTagDecl = dyn_cast_or_null<TagDecl>(PrevDecl)) {
    // C++11 [class.mem]p1:
    //   A member shall not be declared twice in the member-specification,
    //   except that a nested class or member class template can be declared
    //   and then later defined.
    if (!D->hasDefinition() && PrevDecl->isCXXClassMember()) {
      SemaRef.Diag(DNI.getLoc(), diag::ext_member_redeclared);
      SemaRef.Diag(PrevTagDecl->getLocation(), diag::note_previous_declaration);
    }

    if (!Invalid) {
      // Diagnose attempts to redefine a tag.
      if (D->hasDefinition()) {
        if (NamedDecl *Def = PrevTagDecl->getDefinition()) {
          SemaRef.Diag(DNI.getLoc(), diag::err_redefinition) << DNI.getName();
          SemaRef.notePreviousDefinition(Def, DNI.getLoc());
          Invalid = true;
        }
      }
    }
  }
}

static CXXRecordDecl *InjectClassDecl(InjectionContext &Ctx, DeclContext *Owner,
                                      CXXRecordDecl *D) {
  Sema &SemaRef = Ctx.getSema();

  bool Invalid = false;

  // This is a bit weird, but we need to delay type creation
  // if we're injecting a class decl for a template,
  // to ensure the type has knowledge that it's for a templated
  // class, rather than a normal class.
  //
  // This has immediately visible impact, with regards to
  // constructors of injected classes, as these are not resolveable
  // without delaying type creation.
  bool DelayTypeCreation = D->getDescribedClassTemplate();

  CXXRecordDecl *Class;
  if (D->isInjectedClassName()) {
    DeclarationName DN = cast<CXXRecordDecl>(Owner)->getDeclName();
    Class = CXXRecordDecl::Create(
        Ctx.getContext(), D->getTagKind(), Owner, D->getBeginLoc(),
        D->getLocation(), DN.getAsIdentifierInfo(),
        /*PrevDecl=*/nullptr, DelayTypeCreation);
  } else {
    DeclarationNameInfo DNI = Ctx.TransformDeclarationName(D);

    NamedDecl *PrevDecl;
    CheckInjectedTagDecl(SemaRef, DNI, Owner, D, PrevDecl, Invalid);

    Class = CXXRecordDecl::Create(
        Ctx.getContext(), D->getTagKind(), Owner, D->getBeginLoc(),
        D->getLocation(), DNI.getName().getAsIdentifierInfo(),
        cast_or_null<CXXRecordDecl>(PrevDecl), DelayTypeCreation);
  }
  Ctx.AddDeclSubstitution(D, Class);

  // FIXME: Inject attributes.

  // FIXME: Propagate other properties?
  Class->setAccess(D->getAccess());
  Class->setImplicit(D->isImplicit());
  Class->setInvalidDecl(Invalid);

  return Class;
}

static bool ShouldImmediatelyInjectPendingDefinitions(
              Decl *Injectee, DeclContext *ClassOwner, bool InjectedIntoOwner) {
  // If we're injecting into a class, always defer.
  if (isa<CXXRecordDecl>(Injectee) && InjectedIntoOwner)
    return false;

  // Handle pending members, we've reached the injectee.
  if (Decl::castFromDeclContext(ClassOwner) == Injectee)
    return true;

  // Handle pending members, we've reached the root of the rebuild injection.
  if (!InjectedIntoOwner)
    return true;

  return false;
}

static void InjectPendingDefinitionsWithCleanup(InjectionContext &Ctx) {
  InjectionInfo *Injection = Ctx.CurInjection;

  InjectPendingDefinitions<FieldDecl, InjectedDef_Field>(&Ctx, Injection);
  InjectPendingDefinitions<FunctionTemplateDecl, CXXMethodDecl,
                           InjectedDef_TemplatedMethod>(&Ctx, Injection);
  InjectPendingDefinitions<CXXMethodDecl, InjectedDef_Method>(&Ctx, Injection);
  InjectPendingDefinitions<FunctionDecl, InjectedDef_FriendFunction>(&Ctx, Injection);

  Injection->ResetClassMemberData();
}

static void InjectClassDefinition(InjectionContext &Ctx, DeclContext *Owner,
                                  CXXRecordDecl *D, CXXRecordDecl *Class,
                                  bool InjectIntoOwner) {
  if (D->hasDefinition())
    InjectClassDefinition(Ctx, D, Class);

  if (ShouldImmediatelyInjectPendingDefinitions(
        Ctx.Injectee, Owner, InjectIntoOwner)) {
    InjectPendingDefinitionsWithCleanup(Ctx);
    Ctx.getSema().InjectPendingNamespaceInjections();
  }
}

Decl *InjectionContext::InjectCXXRecordDecl(CXXRecordDecl *D) {
  DeclContext *Owner = getSema().CurContext;
  CXXRecordDecl *Class = InjectClassDecl(*this, Owner, D);

  // Don't register the declaration if we're merely attempting to transform
  // this class.
  bool InjectIntoOwner = ShouldInjectInto(Owner);
  if (InjectIntoOwner)
    Owner->addDecl(Class);

  InjectClassDefinition(*this, Owner, D, Class, InjectIntoOwner);

  return Class;
}

// FIXME: This needs a LOT of work.
Decl* InjectionContext::InjectStaticDataMemberDecl(FieldDecl *D) {
  DeclarationNameInfo DNI;
  TypeSourceInfo *TSI;
  CXXRecordDecl *Owner;
  bool Invalid = InjectMemberDeclarator(D, DNI, TSI, Owner);

  VarDecl *Var = VarDecl::Create(
      getContext(), Owner, D->getLocation(), DNI.getLoc(), DNI.getName(),
      TSI->getType(), TSI, SC_Static);
  AddDeclSubstitution(D, Var);

  ApplyAccess(GetModifiers(), Var, D);
  Var->setInvalidDecl(Invalid);
  Owner->addDecl(Var);

  // FIXME: This is almost certainly going to break when it runs.
  // if (D->hasInClassInitializer())
  //   InjectedDefinitions.push_back(InjectedDef(D, Var));

  if (D->hasInClassInitializer())
    llvm_unreachable("Initialization of static members not implemented");

  return Var;
}

static NamedDecl *getPreviousFieldDecl(Sema &SemaRef, FieldDecl *FD,
                                       DeclContext *Owner) {
  // FIXME: Is this right?
  LookupResult Previous(
    SemaRef, FD->getDeclName(), FD->getLocation(),
    Sema::LookupMemberName, SemaRef.forRedeclarationInCurContext());
  SemaRef.LookupQualifiedName(Previous, Owner);

  NamedDecl *PrevDecl = nullptr;
  switch (Previous.getResultKind()) {
    case LookupResult::Found:
    case LookupResult::FoundUnresolvedValue:
      PrevDecl = Previous.getAsSingle<NamedDecl>();
      break;

    case LookupResult::FoundOverloaded:
      PrevDecl = Previous.getRepresentativeDecl();
      break;

    case LookupResult::NotFound:
    case LookupResult::NotFoundInCurrentInstantiation:
    case LookupResult::Ambiguous:
      break;
  }
  Previous.suppressDiagnostics();

  if (PrevDecl && !SemaRef.isDeclInScope(PrevDecl, Owner))
    PrevDecl = nullptr;

  return PrevDecl;
}

Decl *InjectionContext::InjectFieldDecl(FieldDecl *D) {
  if (GetModifiers().getStorageModifier() == StorageModifier::Static) {
    return InjectStaticDataMemberDecl(D);
  }

  DeclarationNameInfo DNI;
  TypeSourceInfo *TSI;
  CXXRecordDecl *Owner;
  bool Invalid = InjectMemberDeclarator(D, DNI, TSI, Owner);

  // Substitute through the bit width.
  Expr *BitWidth = nullptr;
  {
    // The bit-width expression is a constant expression.
    EnterExpressionEvaluationContext Unevaluated(
        SemaRef, Sema::ExpressionEvaluationContext::ConstantEvaluated);

    ExprResult NewBitWidth = TransformExpr(D->getBitWidth());
    if (NewBitWidth.isInvalid()) {
      Invalid = true;
    } else {
      BitWidth = NewBitWidth.get();
    }
  }

  NamedDecl *PrevDecl = getPreviousFieldDecl(SemaRef, D, Owner);

  // Build and check the field.
  FieldDecl *Field = getSema().CheckFieldDecl(
      DNI.getName(), TSI->getType(), TSI, Owner, D->getLocation(),
      D->isMutable(), BitWidth, D->getInClassInitStyle(), D->getInnerLocStart(),
      D->getAccess(), PrevDecl);
  AddDeclSubstitution(D, Field);

  // FIXME: Propagate attributes?

  // FIXME: In general, see VisitFieldDecl in the template instantiatior.
  // There are some interesting cases we probably need to handle.

  // Can't make
  if (GetModifiers().modifyConstexpr()) {
    SemaRef.Diag(D->getLocation(), diag::err_modify_constexpr_field) <<
        Transform(GetModifiers().getConstexprModifier());
    Field->setInvalidDecl(true);
  }

  // Propagate semantic properties.
  Field->setImplicit(D->isImplicit());
  ApplyAccess(GetModifiers(), Field, D);

  if (!Field->isInvalidDecl())
    Field->setInvalidDecl(Invalid);

  Owner->addDecl(Field);

  // If the field has an initializer, add it to the Fragment so that we
  // can process it later.
  if (D->hasInClassInitializer())
    AddPendingDefinition(InjectedDef(InjectedDef_Field, D, Field));

  // Mark that we've injected a field.
  InjectedFieldData();

  return Field;
}

template<typename T>
static ExplicitSpecifier getExplicit(T *D, ReflectionModifiers &Mods) {
  ExplicitSpecifier Spec = D->getExplicitSpecifier();
  if (Mods.addExplicit())
    Spec.setKind(ExplicitSpecKind::ResolvedTrue);
  return Spec;
}

template<typename F>
Decl *InjectionContext::InjectCXXMethodDecl(CXXMethodDecl *D, F FinishBody) {
  ASTContext &AST = getContext();
  DeclarationNameInfo DNI;
  TypeSourceInfo *TSI;
  CXXRecordDecl *Owner;
  bool Invalid = InjectMemberDeclarator(D, DNI, TSI, Owner);

  // Build the underlying method.
  //
  // FIXME: Should we propagate implicit operators?
  CXXMethodDecl *Method;
  if (CXXConstructorDecl *Ctor = dyn_cast<CXXConstructorDecl>(D)) {
    Method = CXXConstructorDecl::Create(AST, Owner, D->getBeginLoc(), DNI,
                                        TSI->getType(), TSI,
                                        getExplicit(Ctor, GetModifiers()),
                                        Ctor->isInlineSpecified(),
                                        Ctor->isImplicit(),
                                        Ctor->getConstexprKind());
    Method->setRangeEnd(D->getEndLoc());
  } else if (CXXDestructorDecl *Dtor = dyn_cast<CXXDestructorDecl>(D)) {
    Method = CXXDestructorDecl::Create(AST, Owner, D->getBeginLoc(), DNI,
                                       TSI->getType(), TSI,
                                       Dtor->isInlineSpecified(),
                                       Dtor->isImplicit(),
                                       Dtor->getConstexprKind());
    Method->setRangeEnd(D->getEndLoc());
  } else if (CXXConversionDecl *Conv = dyn_cast<CXXConversionDecl>(D)) {
    Method = CXXConversionDecl::Create(AST, Owner, D->getBeginLoc(), DNI,
                                       TSI->getType(), TSI,
                                       Conv->isInlineSpecified(),
                                       getExplicit(Conv, GetModifiers()),
                                       Conv->getConstexprKind(),
                                       Conv->getEndLoc());
  } else {
    Method = CXXMethodDecl::Create(AST, Owner, D->getBeginLoc(), DNI,
                                   TSI->getType(), TSI,
                                   D->isStatic() ? SC_Static : SC_None,
                                   D->isInlineSpecified(),
                                   D->getConstexprKind(),
                                   D->getEndLoc());
  }
  AddDeclSubstitution(D, Method);
  UpdateFunctionParms(D, Method);

  // Propagate Template Attributes
  MemberSpecializationInfo *MemberSpecInfo = D->getMemberSpecializationInfo();
  if (MemberSpecInfo) {
    TemplateSpecializationKind TemplateSK =
        MemberSpecInfo->getTemplateSpecializationKind();
    FunctionDecl *TemplateFD =
        static_cast<FunctionDecl *>(MemberSpecInfo->getInstantiatedFrom());
    Method->setInstantiationOfMemberFunction(TemplateFD, TemplateSK);
  }

  // Propagate semantic properties.
  if (D->isInlined())
    Method->setImplicitlyInline();

  Method->setImplicit(D->isImplicit());
  ApplyAccess(GetModifiers(), Method, D);

  // Update the constexpr specifier.
  if (GetModifiers().modifyConstexpr()) {
    Method->setType(Method->getType().withConst());

    ConstexprModifier Modifier = GetModifiers().getConstexprModifier();
    if (Modifier == ConstexprModifier::Constinit) {
      SemaRef.Diag(Method->getLocation(), diag::err_modify_constinit_function);
      Method->setInvalidDecl(true);
    } else {
      ConstexprSpecKind SpecKind = Transform(Modifier);
      if (isa<CXXDestructorDecl>(Method)) {
        SemaRef.Diag(D->getLocation(), diag::err_constexpr_dtor) << SpecKind;
        Method->setInvalidDecl(true);
      }
      Method->setConstexprKind(SpecKind);
    }
  } else {
    Method->setConstexprKind(D->getConstexprKind());
  }

  // Propagate virtual flags.
  Method->setVirtualAsWritten(D->isVirtualAsWritten());
  if (D->isPure())
    SemaRef.CheckPureMethod(Method, Method->getSourceRange());

  // Request to make function virtual. Note that the original may have
  // a definition. When the original is defined, we'll ignore the definition.
  if (GetModifiers().addVirtual() || GetModifiers().addPureVirtual()) {
    // FIXME: Actually generate a diagnostic here.
    if (isa<CXXConstructorDecl>(Method)) {
      SemaRef.Diag(D->getLocation(), diag::err_modify_virtual_constructor);
      Method->setInvalidDecl(true);
    } else {
      Method->setVirtualAsWritten(true);
      if (GetModifiers().addPureVirtual())
        SemaRef.CheckPureMethod(Method, Method->getSourceRange());
    }
  }

  if (OverrideAttr *OA = D->getAttr<OverrideAttr>())
    Method->addAttr(OA);
  if (FinalAttr *FA = D->getAttr<FinalAttr>())
    Method->addAttr(FA);

  Method->setDeletedAsWritten(D->isDeletedAsWritten());
  Method->setDefaulted(D->isDefaulted());
  Method->setExplicitlyDefaulted(D->isExplicitlyDefaulted());

  if (!Method->isInvalidDecl())
    Method->setInvalidDecl(Invalid);

  // Don't register the declaration if we're merely attempting to transform
  // this method.
  if (ShouldInjectInto(Owner)) {
    CheckInjectedFunctionDecl(getSema(), Method, Owner);
    Owner->addDecl(Method);
  }

  // If the method is has a body, create an appropriate pending definition,
  // so that we can process it later. Note that deleted/defaulted
  // definitions are just flags processed above. Ignore the definition
  // if we've marked this as pure virtual.
  if (!Method->isPure()) {
    FinishBody(Method);
  }

  return Method;
}

Decl *InjectionContext::InjectCXXMethodDecl(CXXMethodDecl *D) {
  return InjectCXXMethodDecl(D, [&](CXXMethodDecl *Method) {
    // FIXME: This logic is nearly identical to that used for
    // CreatePartiallySubstPattern.

    // FIXME: We reuse willHaveBody, is that okay?

    // If this function has a body add a pending definition that translates it into
    // the new method. If this function has been previously seen i.e. will have a body
    // then create a similar pending definition. This is required as during a template
    // instantiation, there are two injections, one for the the template substitutions
    // then one for the actual injection.
    if (D->hasBody() || D->willHaveBody()) {
       Method->setWillHaveBody();
       AddPendingDefinition(InjectedDef(InjectedDef_Method, D, Method));
    }
  });
}

Decl *InjectionContext::InjectDeclImpl(Decl *D) {
  // Inject the declaration.
  switch (D->getKind()) {
  case Decl::Namespace:
    return InjectNamespaceDecl(cast<NamespaceDecl>(D));
  case Decl::Typedef:
  case Decl::TypeAlias:
    return InjectTypedefNameDecl(cast<TypedefNameDecl>(D));
  case Decl::Function:
    return InjectFunctionDecl(cast<FunctionDecl>(D));
  case Decl::Var:
    return InjectVarDecl(cast<VarDecl>(D));
  case Decl::CXXRecord:
    return InjectCXXRecordDecl(cast<CXXRecordDecl>(D));
  case Decl::Field:
    return InjectFieldDecl(cast<FieldDecl>(D));
  case Decl::CXXMethod:
  case Decl::CXXConstructor:
  case Decl::CXXDestructor:
  case Decl::CXXConversion:
    return InjectCXXMethodDecl(cast<CXXMethodDecl>(D));
  case Decl::AccessSpec:
    return InjectAccessSpecDecl(cast<AccessSpecDecl>(D));
  case Decl::Friend:
    return InjectFriendDecl(cast<FriendDecl>(D));
  case Decl::CXXMetaprogram:
    return InjectCXXMetaprogramDecl(cast<CXXMetaprogramDecl>(D));
  case Decl::CXXInjection:
    return InjectCXXInjectionDecl(cast<CXXInjectionDecl>(D));
  case Decl::ClassTemplate:
    return InjectClassTemplateDecl(cast<ClassTemplateDecl>(D));
  case Decl::ClassTemplateSpecialization:
    return InjectClassTemplateSpecializationDecl(
               cast<ClassTemplateSpecializationDecl>(D));
  case Decl::FunctionTemplate:
    return InjectFunctionTemplateDecl(cast<FunctionTemplateDecl>(D));
  case Decl::TemplateTypeParm:
    return InjectTemplateTypeParmDecl(cast<TemplateTypeParmDecl>(D));
  case Decl::NonTypeTemplateParm:
    return InjectNonTypeTemplateParmDecl(cast<NonTypeTemplateParmDecl>(D));
  case Decl::TemplateTemplateParm:
    return InjectTemplateTemplateParmDecl(cast<TemplateTemplateParmDecl>(D));
  case Decl::StaticAssert:
    return InjectStaticAssertDecl(cast<StaticAssertDecl>(D));
  case Decl::Enum:
    return InjectEnumDecl(cast<EnumDecl>(D));
  case Decl::EnumConstant:
    return InjectEnumConstantDecl(cast<EnumConstantDecl>(D));
  case Decl::CXXRequiredType:
    return InjectCXXRequiredTypeDecl(cast<CXXRequiredTypeDecl>(D));
  case Decl::CXXRequiredDeclarator:
    return InjectCXXRequiredDeclaratorDecl(cast<CXXRequiredDeclaratorDecl>(D));
  default:
    break;
  }
  D->dump();
  llvm_unreachable("unhandled declaration");
}

/// Injects a new version of the declaration.
Decl *InjectionContext::InjectDecl(Decl *D) {
  if (Decl *Replacement = GetDeclReplacement(D))
    return Replacement;

  // If the declaration does not appear in the context, then it need
  // not be resolved.
  if (!isInInjection(D))
    return D;

  Decl* R = InjectDeclImpl(D);
  if (!R || R->isInvalidDecl())
    return R;

  // Ensure we've actually made an effort to rebuilt the decl.
  assert(R != D);

  // If we injected a top-level declaration, notify the AST consumer,
  // so that it can be processed for code generation.
  //
  // Avoid doing this if only rebuilding the declaration.
  if (R->getDeclContext()->isFileContext() && !isRebuildOnly())
    getSema().Consumer.HandleTopLevelDecl(DeclGroupRef(R));

  return R;
}

Decl *InjectionContext::RebuildInjectDecl(Decl *D) {
  RebuildOnlyContextRAII RebuildCtx(*this);

  // Run normal injection logic.
  return InjectDecl(D);
}

Decl *InjectionContext::InjectAccessSpecDecl(AccessSpecDecl *D) {
  CXXRecordDecl *Owner = cast<CXXRecordDecl>(getSema().CurContext);
  return AccessSpecDecl::Create(
      getContext(), D->getAccess(), Owner, D->getLocation(), D->getColonLoc());
}

static bool GetFriendTargetDeclContext(Sema &SemaRef, FriendDecl *D, DeclContext *DOwner,
                                       Decl *ND, DeclContext *&DC) {
  if (auto *FD = dyn_cast<FunctionDecl>(ND)) {
    LookupResult Previous(
                          SemaRef, FD->getDeclName(), SourceLocation(),
                          Sema::LookupOrdinaryName, SemaRef.forRedeclarationInCurContext());

    Scope *S = SemaRef.getScopeForContext(DOwner);
    CXXScopeSpec SS;
    SS.Adopt(FD->getQualifierLoc());

    Scope *DCScope = nullptr;
    return SemaRef.GetFriendFunctionDC(Previous, S, SS, FD->getNameInfo(),
                                       D->getFriendLoc(), D->getLocation(),
                                       FD->hasBody(), isa<FunctionTemplateDecl>(FD), DC, DCScope);
  }

  DC = DOwner;
  return false;
}

Decl *InjectionContext::InjectFriendDecl(FriendDecl *D) {
  CXXRecordDecl *Owner = cast<CXXRecordDecl>(getSema().CurContext);

  if (TypeSourceInfo *Ty = D->getFriendType()) {
    TypeSourceInfo *InstTy;
    if (D->isUnsupportedFriend()) {
      InstTy = Ty;
    } else {
      InstTy = TransformType(Ty);
    }
    if (!InstTy)
      return nullptr;

    FriendDecl *FD = SemaRef.CheckFriendTypeDecl(D->getBeginLoc(),
                                                 D->getFriendLoc(), InstTy);
    if (!FD)
      return nullptr;

    FD->setAccess(AS_public);
    FD->setUnsupportedFriend(D->isUnsupportedFriend());
    Owner->addDecl(FD);
    return FD;
  }

  Decl *ND = RebuildInjectDecl(D->getFriendDecl());

  DeclContext *NDDC = nullptr;
  if (GetFriendTargetDeclContext(SemaRef, D, Owner, ND, NDDC))
    return nullptr;

  ND->setDeclContext(NDDC);
  NDDC->addDecl(ND);

  FriendDecl *FD =
    FriendDecl::Create(SemaRef.Context, Owner, D->getLocation(),
                       cast<NamedDecl>(ND), D->getFriendLoc());

  FD->setAccess(AS_public);
  FD->setUnsupportedFriend(D->isUnsupportedFriend());
  Owner->addDecl(FD);
  return FD;
}

Stmt *InjectionContext::InjectStmt(Stmt *S) {
  Stmt *NewS = InjectStmtImpl(S);

  return NewS;
}

Stmt *InjectionContext::InjectStmtImpl(Stmt *S) {
  switch (S->getStmtClass()) {
  case Stmt::DeclStmtClass:
    return InjectDeclStmt(cast<DeclStmt>(S));
  default:
    // Some statements will not require any special logic.
    StmtResult NewS = TransformStmt(S);
    if (NewS.isInvalid())
      return nullptr;
    InjectedStmts.push_back(NewS.get());
    return NewS.get();
  }
}

static bool isRequiresDecl(Decl *D) {
  return isa<CXXRequiredTypeDecl>(D) || isa<CXXRequiredDeclaratorDecl>(D);
}

static bool isRequiresDecl(DeclStmt *DS) {
  if (!DS->isSingleDecl())
    return false;

  return isRequiresDecl(DS->getSingleDecl());
}

static Decl *InjectDeclStmtDecl(InjectionContext &Ctx, Decl *OldDecl) {
  Decl *D = Ctx.InjectDecl(OldDecl);
  if (!D || D->isInvalidDecl())
    return nullptr;

  return D;
}

static void PushDeclStmtDecl(InjectionContext &Ctx, Decl *NewDecl) {
  Sema &SemaRef = Ctx.getSema();

  // Add the declaration to scope, we don't need to add it to the context,
  // as this should have been handled by the injection of the decl.
  Scope *FunctionScope =
    SemaRef.getScopeForContext(Decl::castToDeclContext(Ctx.Injectee));
  if (!FunctionScope)
    return;

  SemaRef.PushOnScopeChains(cast<NamedDecl>(NewDecl), FunctionScope,
                            /*AddToContext=*/false);
}

static bool InjectDeclStmtDecls(InjectionContext &Ctx, DeclStmt *S,
                                llvm::SmallVectorImpl<Decl *> &Decls) {
  for (Decl *D : S->decls()) {
    if (Decl *NewDecl = InjectDeclStmtDecl(Ctx, D)) {
      Decls.push_back(NewDecl);

      if (isRequiresDecl(NewDecl))
        continue;

      PushDeclStmtDecl(Ctx, NewDecl);
      continue;
    }

    return true;
  }

  return false;
}

Stmt *InjectionContext::InjectDeclStmt(DeclStmt *S) {
  llvm::SmallVector<Decl *, 4> Decls;
  if (InjectDeclStmtDecls(*this, S, Decls))
    return nullptr;

  StmtResult Res = RebuildDeclStmt(Decls, S->getBeginLoc(), S->getEndLoc());
  if (Res.isInvalid())
    return nullptr;

  DeclStmt *NewStmt = cast<DeclStmt>(Res.get());
  if (!isRequiresDecl(NewStmt) || isRebuildOnly())
    InjectedStmts.push_back(NewStmt);
  return NewStmt;
}

// FIXME: To preserve Injector type these have to be slightly different methods
// it would be nice to combine this logic again in a way that preserves type.

Decl *InjectionContext::InjectCXXMetaprogramDecl(CXXMetaprogramDecl *D) {
  Sema &Sema = getSema();

  // We can use the ActOn* members since the initial parsing for these
  // declarations is trivial (i.e., don't have to translate declarators).
  Decl *New = Sema.ActOnCXXMetaprogramDecl(D->getLocation());

  DeclContext *OriginalDC;
  Sema.ActOnStartCXXMetaprogramDecl(New, OriginalDC);

  StmtResult S = TransformStmt(D->getBody());
  if (!S.isInvalid())
    Sema.ActOnFinishCXXMetaprogramDecl(New, S.get(), OriginalDC);
  else
    Sema.ActOnCXXMetaprogramDeclError(New, OriginalDC);

  return New;
}

Decl *InjectionContext::InjectCXXInjectionDecl(CXXInjectionDecl *D) {
  Sema &Sema = getSema();

  // We can use the ActOn* members since the initial parsing for these
  // declarations is trivial (i.e., don't have to translate declarators).
  Decl *New = Sema.ActOnCXXInjectionDecl(D->getLocation());

  DeclContext *OriginalDC;
  Sema.ActOnStartCXXInjectionDecl(New, OriginalDC);

  // Transform the injection stmt rather than the entire body
  // because this is originally built via a single injection statement
  // rather than the body of the function representation.
  //
  // While either would result in a logically correct program, this ensures
  // pretty printing gives a sane result.
  StmtResult S = TransformStmt(D->getInjectionStmt());
  if (!S.isInvalid())
    Sema.ActOnFinishCXXInjectionDecl(New, S.get(), OriginalDC);
  else
    Sema.ActOnCXXInjectionDeclError(New, OriginalDC);

  return New;
}

TemplateParameterList *
InjectionContext::InjectTemplateParms(TemplateParameterList *OldParms) {
  bool Invalid = false;
  SmallVector<NamedDecl *, 8> NewParms;
  NewParms.reserve(OldParms->size());
  for (auto &P : *OldParms) {
    NamedDecl *D = cast_or_null<NamedDecl>(InjectDecl(P));
    NewParms.push_back(D);
    if (!D || D->isInvalidDecl())
      Invalid = true;
  }

  // Clean up if we had an error.
  if (Invalid)
    return nullptr;

  ExprResult Reqs = TransformExpr(OldParms->getRequiresClause());
  if (Reqs.isInvalid())
    return nullptr;

  return TemplateParameterList::Create(
      getSema().Context, OldParms->getTemplateLoc(), OldParms->getLAngleLoc(),
      NewParms, OldParms->getRAngleLoc(), Reqs.get());
}

Decl *InjectionContext::InjectClassTemplateDecl(ClassTemplateDecl *D) {
  DeclContext *Owner = getSema().CurContext;

  TemplateParameterList *Parms = InjectTemplateParms(D->getTemplateParameters());
  if (!Parms)
    return nullptr;

  // Build the underlying pattern.
  CXXRecordDecl *OriginalClass = D->getTemplatedDecl();
  CXXRecordDecl *Class = InjectClassDecl(*this, Owner, OriginalClass);
  if (!Class)
    return nullptr;

  // Build the enclosing template.
  ClassTemplateDecl *Template = ClassTemplateDecl::Create(
       getSema().Context, getSema().CurContext, Class->getLocation(),
       Class->getDeclName(), Parms, Class);
  AddDeclSubstitution(D, Template);

  // FIXME: Other attributes to process?
  Class->setDescribedClassTemplate(Template);

  // Build the type for the class template declaration now.
  QualType T = Template->getInjectedClassNameSpecialization();
  T = getSema().Context.getInjectedClassNameType(Class, T);

  ApplyAccess(GetModifiers(), Template, D);

  // Don't register the declaration if we're merely attempting to transform
  // this template.
  bool InjectIntoOwner = ShouldInjectInto(Owner);
  if (InjectIntoOwner)
    Owner->addDecl(Template);

  InjectClassDefinition(*this, Owner, OriginalClass, Class, InjectIntoOwner);

  return Template;
}

Decl *InjectionContext::InjectClassTemplateSpecializationDecl(
                                           ClassTemplateSpecializationDecl *D) {
  DeclContext *Owner = getSema().CurContext;

  Decl *Template = RebuildInjectDecl(D->getSpecializedTemplate());
  if (!Template)
    return nullptr;
  ClassTemplateDecl *ClassTemplate = cast<ClassTemplateDecl>(Template);

  ArrayRef<TemplateArgument> Args = D->getTemplateInstantiationArgs().asArray();

  // Build the enclosing template.
  ClassTemplateSpecializationDecl *TemplateSpecialization
    = ClassTemplateSpecializationDecl::Create(
       getSema().Context, D->getTagKind(), getSema().CurContext,
       ClassTemplate->getBeginLoc(), ClassTemplate->getLocation(),
       ClassTemplate, Args, nullptr);
  AddDeclSubstitution(D, TemplateSpecialization);

  // FIXME: Other attributes to process?
  TemplateSpecialization->setInstantiationOf(ClassTemplate);

  void *InsertPos = nullptr;
  ClassTemplate->findSpecialization(Args, InsertPos);
  if (InsertPos != nullptr)
    ClassTemplate->AddSpecialization(TemplateSpecialization, InsertPos);

  // Add the declaration.
  Owner->addDecl(TemplateSpecialization);

  if (D->hasDefinition())
    InjectClassDefinition(*this, D, TemplateSpecialization);

  return Template;
}

// Create a partially substituted pattern based off the give multi level
// function template. This route creates a new pattern for a template,
// based of an existing template for instance:
//
// template<typename T>
// struct outer {
//   template<typename K>
//   void foo() {
//     T t;
//   }
// };
//
// This function would be used to create a new `outer<int>::foo`,
// with T replaced such that the new version of foo is
// equivalent to the following `outer::foo`:
//
// struct outer {
//   template<typename K>
//   void foo() {
//     int t;
//   }
// };
//
Decl *InjectionContext::CreatePartiallySubstPattern(FunctionTemplateDecl *D) {
  RebuildOnlyContextRAII RebuildCtx(*this, /*InstantiateTemplates=*/true);

  FunctionDecl *Function = D->getTemplatedDecl();
  return InjectCXXMethodDecl(cast<CXXMethodDecl>(Function), [&](CXXMethodDecl *Method) {
    // FIXME: This logic is nearly identical to that used for
    // CreatePartiallySubstPattern.

    // FIXME: We reuse willHaveBody, is that okay?

    // FIXME: We can probably better optimize this situation to reduce the amount
    // of tree transforms.

    // If this function has a body add a pending definition that translates it into
    // the new method. If this function has been previously seen i.e. will have a body
    // then create a similar pending definition. This is required as during a template
    // instantiation, there are two injections, one for the the template substitutions
    // then one for the actual injection.
    if (Function->hasBody() || Function->willHaveBody()) {
      Method->setWillHaveBody();
      AddPendingDefinition(InjectedDef(InjectedDef_TemplatedMethod, D, Method));
    }
  });
}


Decl *InjectionContext::InjectFunctionTemplateDecl(FunctionTemplateDecl *D) {
  DeclContext *Owner = getSema().CurContext;

  TemplateParameterList *Parms = InjectTemplateParms(D->getTemplateParameters());
  if (!Parms)
    return nullptr;

  // Build the underlying pattern.
  Decl *Pattern;

  if (isa<CXXMethodDecl>(D->getTemplatedDecl())) {
    Pattern = CreatePartiallySubstPattern(D);
  } else {
    Pattern = RebuildInjectDecl(D->getTemplatedDecl());
  }

  if (!Pattern)
    return nullptr;

  FunctionDecl *Fn = cast<FunctionDecl>(Pattern);

  // Build the enclosing template.
  FunctionTemplateDecl *Template = FunctionTemplateDecl::Create(
      getSema().Context, getSema().CurContext, Fn->getLocation(),
      Fn->getDeclName(), Parms, Fn);
  AddDeclSubstitution(D, Template);

  // FIXME: Other attributes to process?
  Fn->setDescribedFunctionTemplate(Template);
  ApplyAccess(GetModifiers(), Template, D);

  // Add the declaration.
  Owner->addDecl(Template);

  return Template;
}

Decl *InjectionContext::InjectTemplateTypeParmDecl(TemplateTypeParmDecl *D) {
  TemplateTypeParmDecl *Parm = TemplateTypeParmDecl::Create(
      getSema().Context, getSema().CurContext, D->getBeginLoc(), D->getLocation(),
      D->getDepth() - getLevelsSubstituted(), D->getIndex(), D->getIdentifier(),
      D->wasDeclaredWithTypename(), D->isParameterPack());
  AddDeclSubstitution(D, Parm);

  if (const CXXRecordDecl *RD = dyn_cast<CXXRecordDecl>(Parm->getDeclContext()))
    Parm->setAccess(RD->getDefaultAccessSpecifier());

  // Process the default argument.
  if (D->hasDefaultArgument() && !D->defaultArgumentWasInherited()) {
    TypeSourceInfo *Default = TransformType(D->getDefaultArgumentInfo());
    if (!Default)
      return nullptr;

    Parm->setDefaultArgument(Default);
  }

  return Parm;
}

Decl *InjectionContext::InjectNonTypeTemplateParmDecl(NonTypeTemplateParmDecl *D) {
  TypeSourceInfo *DI = TransformType(D->getTypeSourceInfo());
  if (!DI)
    return nullptr;

  QualType T = getSema().CheckNonTypeTemplateParameterType(DI, D->getLocation());
  if (T.isNull())
    return nullptr;

  NonTypeTemplateParmDecl *Parm = NonTypeTemplateParmDecl::Create(
      getSema().Context, getSema().CurContext, D->getInnerLocStart(),
      D->getLocation(), D->getDepth() - getLevelsSubstituted(),
      D->getPosition(), D->getIdentifier(), T, D->isParameterPack(), DI);
  AddDeclSubstitution(D, Parm);

  if (const CXXRecordDecl *RD = dyn_cast<CXXRecordDecl>(Parm->getDeclContext()))
    Parm->setAccess(RD->getDefaultAccessSpecifier());

  if (D->hasDefaultArgument() && !D->defaultArgumentWasInherited()) {
    EnterExpressionEvaluationContext ConstantEvaluated(
        SemaRef, Sema::ExpressionEvaluationContext::ConstantEvaluated);

    ExprResult DefaultArg = TransformExpr(D->getDefaultArgument());
    if (DefaultArg.isInvalid())
      return nullptr;

    Parm->setDefaultArgument(DefaultArg.get());
  }

  return Parm;
}

Decl *InjectionContext::InjectTemplateTemplateParmDecl(
                                                  TemplateTemplateParmDecl *D) {
  TemplateParameterList *Params = TransformTemplateParameterList(
                                                    D->getTemplateParameters());

  TemplateTemplateParmDecl *Parm = TemplateTemplateParmDecl::Create(
      getSema().Context, getSema().CurContext, D->getLocation(),
      D->getDepth() - getLevelsSubstituted(), D->getPosition(),
      D->isParameterPack(), D->getIdentifier(), Params);
  AddDeclSubstitution(D, Parm);

  if (const CXXRecordDecl *RD = dyn_cast<CXXRecordDecl>(Parm->getDeclContext()))
    Parm->setAccess(RD->getDefaultAccessSpecifier());

  if (D->hasDefaultArgument() && !D->defaultArgumentWasInherited()) {
    NestedNameSpecifierLoc QualifierLoc = TransformNestedNameSpecifierLoc(
                             D->getDefaultArgument().getTemplateQualifierLoc());

    CXXScopeSpec SS;
    SS.Adopt(QualifierLoc);
    TemplateName TName = TransformTemplateName(
        SS, D->getDefaultArgument().getArgument().getAsTemplate(),
        D->getDefaultArgument().getTemplateNameLoc());

    if (!TName.isNull())
      Parm->setDefaultArgument(
          getSema().Context,
          TemplateArgumentLoc(TemplateArgument(TName),
                              QualifierLoc,
                              D->getDefaultArgument().getTemplateNameLoc()));
  }

  return Parm;
}

Decl *InjectionContext::InjectStaticAssertDecl(StaticAssertDecl *D) {
  Expr *AssertExpr = D->getAssertExpr();

  // The expression in a static assertion is a constant expression.
  EnterExpressionEvaluationContext Unevaluated(
      SemaRef, Sema::ExpressionEvaluationContext::ConstantEvaluated);

  ExprResult InstantiatedAssertExpr = TransformExpr(AssertExpr);
  if (InstantiatedAssertExpr.isInvalid())
    return nullptr;

  return SemaRef.BuildStaticAssertDeclaration(D->getLocation(),
                                              InstantiatedAssertExpr.get(),
                                              D->getMessage(),
                                              D->getRParenLoc(),
                                              D->isFailed());
}

template<typename T>
static void PushInjectedECD(
    T *MetaDecl, SmallVectorImpl<Decl *> &EnumConstantDecls) {
  EnumConstantDecls.push_back(MetaDecl);

  for (Decl *ECD : MetaDecl->getInjectedDecls()) {
    EnumConstantDecls.push_back(ECD);
  }
}

static void InstantiateEnumDefinition(InjectionContext &Ctx, EnumDecl *Enum,
                                      EnumDecl *Pattern) {
  Sema &SemaRef = Ctx.getSema();

  Enum->startDefinition();

  // Update the location to refer to the definition.
  Enum->setLocation(Pattern->getLocation());

  SmallVector<Decl *, 4> Enumerators;

  EnumConstantDecl *OriginalLastEnumConst = SemaRef.LastEnumConstDecl;
  SemaRef.LastEnumConstDecl = nullptr;

  for (auto *OldEnumerator : Pattern->decls()) {
    // Don't transform invalid declarations.
    if (OldEnumerator->isInvalidDecl())
      continue;

    // Pull in any injected meta decls.
    Decl *NewEnumerator = Ctx.InjectDecl(OldEnumerator);
    if (auto *MD = dyn_cast<CXXInjectorDecl>(NewEnumerator)) {
      PushInjectedECD(MD, Enumerators);
      continue;
    }

    // FIXME: This is really strange.
    //
    // Bypass the normal mechanism for enumerators, inject
    // them immediately.
    assert(Ctx.InjectedDecls.back() == NewEnumerator);
    Ctx.InjectedDecls.pop_back();

    EnumConstantDecl *EC = cast<EnumConstantDecl>(NewEnumerator);
    Enumerators.push_back(EC);
  }

  SemaRef.LastEnumConstDecl = OriginalLastEnumConst;

  SemaRef.ActOnEnumBody(Enum->getLocation(), Enum->getBraceRange(), Enum,
                        Enumerators, nullptr, ParsedAttributesView());
}

Decl *InjectionContext::InjectEnumDecl(EnumDecl *D) {
  DeclContext *Owner = getSema().CurContext;

  DeclarationNameInfo DNI = TransformDeclarationName(D);

  bool Invalid = false;
  NamedDecl *PrevDecl;
  CheckInjectedTagDecl(SemaRef, DNI, Owner, D, PrevDecl, Invalid);

  EnumDecl *Enum = EnumDecl::Create(
      SemaRef.Context, Owner, D->getBeginLoc(), D->getLocation(),
      D->getIdentifier(), /*PrevDecl=*/nullptr, D->isScoped(),
      D->isScopedUsingClassTag(), D->isFixed());
  AddDeclSubstitution(D, Enum);

  Sema::ContextRAII SavedContext(SemaRef, Enum);
  if (D->isFixed()) {
    if (TypeSourceInfo *TI = D->getIntegerTypeSourceInfo()) {
      // If we have type source information for the underlying type, it means it
      // has been explicitly set by the user. Perform substitution on it before
      // moving on.
      TypeSourceInfo *NewTI = TransformType(TI);
      if (!NewTI || SemaRef.CheckEnumUnderlyingType(NewTI))
        Enum->setIntegerType(SemaRef.Context.IntTy);
      else
        Enum->setIntegerTypeSourceInfo(NewTI);
    } else {
      assert(!D->getIntegerType()->isDependentType()
             && "Dependent type without type source info");
      Enum->setIntegerType(D->getIntegerType());
    }
  }

  // Propagate Template Attributes
  MemberSpecializationInfo *MemberSpecInfo = D->getMemberSpecializationInfo();
  if (MemberSpecInfo) {
    TemplateSpecializationKind TemplateSK =
        MemberSpecInfo->getTemplateSpecializationKind();
    EnumDecl *TemplateED =
        static_cast<EnumDecl *>(MemberSpecInfo->getInstantiatedFrom());
    Enum->setInstantiationOfMemberEnum(TemplateED, TemplateSK);
  }

  // Propagate semantic properties.
  ApplyAccess(GetModifiers(), Enum, D);
  if (Invalid)
    Enum->setInvalidDecl(true);

  if (ShouldInjectInto(Owner))
    Owner->addDecl(Enum);

  if (EnumDecl *Def = D->getDefinition())
    InstantiateEnumDefinition(*this, Enum, Def);

  return Enum;
}

Decl *InjectionContext::InjectEnumConstantDecl(EnumConstantDecl *D) {
  DeclContext *Owner = getSema().CurContext;

  EnumDecl *ED = cast<EnumDecl>(Owner);

  // The specified value for the enumerator.
  ExprResult Value((Expr *)nullptr);
  if (Expr *UninstValue = D->getInitExpr()) {
    // The enumerator's value expression is a constant expression.
    EnterExpressionEvaluationContext Unevaluated(
        SemaRef, Sema::ExpressionEvaluationContext::ConstantEvaluated);
    Value = TransformExpr(UninstValue);
  }

  DeclarationNameInfo DNI = TransformDeclarationName(D);
  Decl *EnumConstDecl = getSema().CheckEnumConstant(
      ED, getSema().LastEnumConstDecl, DNI, Value.get());

  EnumConstDecl->setAccess(ED->getAccess());

  bool InjectIntoOwner = ShouldInjectInto(Owner);
  if (InjectIntoOwner) {
    Owner->addDecl(EnumConstDecl);
    InjectedDecls.push_back(EnumConstDecl);
    getSema().LastEnumConstDecl = cast<EnumConstantDecl>(EnumConstDecl);
  }

  return EnumConstDecl;
}

/// Creates the mapping between the CXXRequiredTypeDecl *D,
/// and the corresponding type.
///
/// Returns true on error.
static bool CXXRequiredDeclSubst(InjectionContext &Ctx,
                                 LookupResult &R,
                                 CXXRequiredTypeDecl *D) {
  Sema &SemaRef = Ctx.getSema();

  // More than one UDT by this name was found:
  // we don't know which one to use.
  // TODO: Should we merge the types instead?
  if (!R.isSingleResult()) {
    constexpr unsigned error_id = 0;
    SemaRef.Diag(D->getLocation(),
                 diag::err_ambiguous_required_name) << error_id;
    return false;
  }

  // If we found an unambiguous UDT, we are good to go.
  if (R.isSingleTagDecl()) {
    NamedDecl *FoundName = R.getFoundDecl();
    if (!isa<TypeDecl>(FoundName)) {
      SemaRef.Diag(D->getLocation(), diag::err_using_typename_non_type);
      return true;
    }

    const Type *FoundType = cast<TypeDecl>(FoundName)->getTypeForDecl();
    QualType ResultType(FoundType, 0);
    Ctx.RequiredTypes.insert({D, ResultType});
  } else {
    // The name was found but it wasn't a UDT.
    // FIXME: Are there non-builtin types that aren't TagDecls?
    SemaRef.Diag(D->getLocation(), diag::err_using_typename_non_type);
    return true;
  }

  return false;
}

static bool TypeCheckRequiredDeclarator(Sema &S, Decl *Required, Decl *Found) {
  if (!isa<DeclaratorDecl>(Required) || !isa<DeclaratorDecl>(Found))
    return true;
  /// TODO: diagnostic here
  if (isa<FunctionDecl>(Found) != isa<FunctionDecl>(Required))
    return true;
  if (isa<VarDecl>(Found) != isa<VarDecl>(Required))
    return true;

  bool DiagnoseTypeMismatch = false;
  DeclaratorDecl *RequiredDeclarator = cast<DeclaratorDecl>(Required);
  DeclaratorDecl *FoundDeclarator = cast<DeclaratorDecl>(Found);
  QualType RDDTy = RequiredDeclarator->getType();
  QualType FoundDeclTy = FoundDeclarator->getType();
  SourceLocation RDLoc = RequiredDeclarator->getLocation();

  // Constexpr-ness must match between what we required and what we found.
  if (const FunctionDecl *FoundFD = dyn_cast<FunctionDecl>(FoundDeclarator)) {
    FunctionDecl *FD = cast<FunctionDecl>(RequiredDeclarator);
    if (FD->isConstexpr() != FoundFD->isConstexpr()) {
      S.Diag(RDLoc, diag::err_constexpr_redecl_mismatch)
          << FD << FD->getConstexprKind() << FoundFD->getConstexprKind();
      S.Diag(FoundFD->getLocation(), diag::note_previous_declaration);
      return true;
    }

    if (FD->getReturnType()->getContainedAutoType()) {
      if (S.TypeCheckRequiredAutoReturn(FD->getLocation(),
            FD->getReturnType(), FoundFD->getReturnType()))
        return true;
      DiagnoseTypeMismatch =
        !S.Context.hasSameFunctionTypeIgnoringReturn(RDDTy, FoundDeclTy);
    }
  }
  if (const VarDecl *FoundVD = dyn_cast<VarDecl>(FoundDeclarator)) {
    VarDecl *VD = cast<VarDecl>(RequiredDeclarator);
    if (VD->isConstexpr() != FoundVD->isConstexpr()) {
      S.Diag(RDLoc, diag::err_constexpr_redecl_mismatch) << VD
        << (VD->isConstexpr() ? CSK_constexpr : CSK_unspecified)
        << (FoundVD->isConstexpr() ? CSK_constexpr : CSK_unspecified);
      S.Diag(FoundVD->getLocation(), diag::note_previous_declaration);
      return true;
    }

    DiagnoseTypeMismatch = !S.Context.hasSameType(RDDTy, FoundDeclTy);
  }

  if ((RDDTy->isReferenceType() != FoundDeclTy->isReferenceType())) {
    S.Diag(RDLoc, diag::err_required_decl_mismatch) << RDDTy << FoundDeclTy;
    return true;   // Types must match exactly, down to the specifier.
  }
  if (DiagnoseTypeMismatch) {
    constexpr unsigned error_id = 1;
    S.Diag(RDLoc, diag::err_required_name_not_found) << error_id;
    S.Diag(RDLoc, diag::note_required_bad_conv) << RDDTy << FoundDeclTy;
    return true;
  }

  return false;
}

static const FunctionProtoType *tryGetFunctionProtoType(QualType FromType) {
  if (auto *FPT = FromType->getAs<FunctionProtoType>())
    return FPT;

  if (auto *MPT = FromType->getAs<MemberPointerType>())
    return MPT->getPointeeType()->getAs<FunctionProtoType>();

  return nullptr;
}

/// Called from CXXRequiredDeclaratorDeclSubst, handles the specific case of a
/// function being required, e.g.:
///
/// \code
/// __fragment {
///   requires void foo(int n);
/// };
/// void foo() {}
/// void foo(int n) {}
/// \endcode
///
/// Will fail if the required overload does not match any declaration
/// in the outer scope.
/// Returns true on error.
static bool HandleFunctionDeclaratorSubst(InjectionContext &Ctx,
                                          LookupResult &R,
                                          CXXRequiredDeclaratorDecl *D) {
  Sema &SemaRef = Ctx.getSema();
  TypeSourceInfo *TInfo = D->getDeclaratorTInfo();
  if (!(TInfo->getType()->isFunctionType()))
    return true;

  const FunctionProtoType *FPT = tryGetFunctionProtoType(TInfo->getType());
  if (!FPT)
    llvm_unreachable("Indeterminate required function.");
  // Create some fake parameters.
  llvm::SmallVector<Expr *, 8> Params;
  for (const QualType Ty : FPT->param_types()) {
    ParmVarDecl *Param =
      SemaRef.BuildParmVarDeclForTypedef(Ctx.getInjectionDeclContext(),
                                         D->getLocation(), Ty);
    Param->setScopeInfo(0, Params.size());
    DeclRefExpr *ParamDRE =
      DeclRefExpr::Create(Ctx.getContext(), NestedNameSpecifierLoc(),
                          SourceLocation(), Param, false, SourceLocation(),
                          Ty, VK_LValue);
    Params.push_back(ParamDRE);
  }

  // Now build the call expression to ensure that this overload is valid.
  // if (D->getDeclContext()->isRecord()) {
  //   CXXScopeSpec SS;
  //   Scope *S = SemaRef.getScopeForContext(InjecteeAsDC);
  //   // Use the parsed scope if it is available. If not, look it up.
  //   ParserLookupSetup ParserLookup(SemaRef, SemaRef.CurContext);
  //   if (!S)
  //     S = ParserLookup.getCurScope();
  //   ExprResult IME =
  //     SemaRef.BuildImplicitMemberExpr(SS, SourceLocation(), R, true, S);
  // } else {
  const UnresolvedSetImpl &FoundNames = R.asUnresolvedSet();
  UnresolvedLookupExpr *ULE =
    UnresolvedLookupExpr::Create(SemaRef.Context, nullptr,
                                 D->getQualifierLoc(), D->getNameInfo(),
                                 /*ADL=*/true, /*Overloaded=*/true,
                                 FoundNames.begin(), FoundNames.end());
  // }

  ExprResult CallRes = SemaRef.ActOnCallExpr(nullptr, ULE, SourceLocation(),
                                             Params, SourceLocation());

  if (CallRes.isInvalid() || !isa<CallExpr>(CallRes.get())) {
    SemaRef.Diag(D->getLocation(), diag::err_undeclared_use)
      << "required declarator.";
    return true;
  }

  CallExpr *Call = cast<CallExpr>(CallRes.get());
  FunctionDecl *CalleeDecl = Call->getDirectCallee();
  if (TypeCheckRequiredDeclarator(SemaRef, D->getRequiredDeclarator(),
                                  CalleeDecl))
    return true;
  Ctx.AddDeclSubstitution(D->getRequiredDeclarator(), CalleeDecl);
  return false;
}

/// Creates the mapping between the CXXRequiredDeclaratorDecl *D,
/// and the corresponding type.
///
/// Returns true on error.
static bool CXXRequiredDeclSubst(InjectionContext &Ctx,
                                 LookupResult &R,
                                 CXXRequiredDeclaratorDecl *D) {
  Sema &SemaRef = Ctx.getSema();
  if (R.isSingleResult()) {
    NamedDecl *FoundDecl = R.getFoundDecl();

    if (!FoundDecl || FoundDecl->isInvalidDecl() ||
        !isa<DeclaratorDecl>(FoundDecl))
      return true;

    DeclaratorDecl *FoundDeclarator = cast<DeclaratorDecl>(FoundDecl);
    if (D->getRequiredDeclarator()->getType()->isFunctionType())
      return HandleFunctionDeclaratorSubst(Ctx, R, D);

    if (isa<VarDecl>(D->getRequiredDeclarator())) {
      if (!isa<VarDecl>(FoundDeclarator))
        // TODO: Diagnostic
        return true;

      VarDecl *FoundVD = cast<VarDecl>(FoundDecl);
      VarDecl *ReqVD = cast<VarDecl>(D->getRequiredDeclarator());
      // If this is a required auto variable, deduce its type.
      if (ReqVD->getType()->getContainedAutoType()) {
        // If we don't have an initializer to deduce from, we'll
        // invent one.
        Expr *Init;
        if (FoundVD->getInit())
          Init = FoundVD->getInit();
        else
          Init = new (SemaRef.Context) OpaqueValueExpr(FoundVD->getLocation(),
                         FoundVD->getType().getNonReferenceType(), VK_RValue);
        if (!Init)
          return true;

        QualType DeducedType;
        if (SemaRef.DeduceAutoType(D->getDeclaratorTInfo(), Init, DeducedType)
            == Sema::DAR_Failed) {
          SemaRef.DiagnoseAutoDeductionFailure(ReqVD, Init);
          return true;
        }
        D->getRequiredDeclarator()->setType(DeducedType);
      }
    }

    if (TypeCheckRequiredDeclarator(SemaRef, D->getRequiredDeclarator(),
                                    FoundDeclarator))
      return true;

    Ctx.AddDeclSubstitution(D->getRequiredDeclarator(), FoundDeclarator);
    return false;
  } else if (R.isOverloadedResult()) {
    return HandleFunctionDeclaratorSubst(Ctx, R, D);
  }

  // Unknown case
  return true;
}

/// Performs lookup on a C++ required declaration.
///
/// Returns true upon error.
template<typename DeclType>
static bool CXXRequiredDeclSubstitute(InjectionContext &Ctx, DeclType *D) {
  Sema &SemaRef = Ctx.getSema();

  // FIXME: We shouldn't be initializing this unless necessary, see below.
  ParserLookupSetup ParserLookup(SemaRef, SemaRef.CurContext);

  // If this meta decl was evaluated immediately, in a non dependent
  // state, then we can use the parser scope directly.
  //
  // If not, then we need to use the scope generated by parser lookup
  // setup. This only works when we're not coming from the parser
  // i.e. we're instantiating a template, and our metaprogram decl
  // is now non-dependent. It requires setup of local variables
  // from within tree transform via the fake parsing scope stack.
  Scope *S;
  if (SemaRef.EvaluatingMetaDeclFromParser)
    S = SemaRef.getCurScope();
  else
    S = ParserLookup.getCurScope();

  // Find the name of the declared type and look it up.
  LookupResult R(SemaRef, D->getDeclName(), D->getLocation(),
                 Sema::LookupAnyName);
  if (SemaRef.LookupName(R, S)) {
    return CXXRequiredDeclSubst(Ctx, R, D);
  } else {
    unsigned error_id =
      std::is_same<DeclType, CXXRequiredTypeDecl>::value ? 0 : 1;

    // We didn't find any type with this name.
    SemaRef.Diag(D->getLocation(), diag::err_required_name_not_found)
      << error_id;
    return true;
  }
}

template<typename DeclType>
static void
SubstitueOrMaintainRequiredDecl(InjectionContext &Ctx, DeclContext *Owner,
                                DeclType *NewDecl) {
  // If we're injecting this declaration, we should never add it to the owner
  // as that would result in the declaration showing up in the injectee.
  // Instead, we want to run lookup, and start the substitution process
  // so that this declaration and its corresponding type effectively disappears.
  //
  // On the other hand, if we're rebuilding this declaration for later use,
  // we do need to add it to the declaration, so that when this fragment is
  // later injected we can run lookup, and start the substitution process.
  //
  // Note that we can't perform lookup prior to the time that we're truly
  // injecting this, as that lookup would potentially occur in the wrong
  // context.

  if (!Ctx.isRebuildOnly()) {
    if (CXXRequiredDeclSubstitute(Ctx, NewDecl)) {
       NewDecl->setInvalidDecl(true);
    }
  } else if (Ctx.ShouldInjectInto(Owner)) {
    Owner->addDecl(NewDecl);
  }
}

Decl *InjectionContext::InjectCXXRequiredTypeDecl(CXXRequiredTypeDecl *D) {
  DeclContext *Owner = getSema().CurContext;

  DeclarationNameInfo DNI = TransformDeclarationName(D);
  IdentifierInfo *Id = DNI.getName().getAsIdentifierInfo();

  auto *RTD = CXXRequiredTypeDecl::Create(
      getContext(), Owner, D->getRequiresLoc(),
      D->getSpecLoc(), Id, D->wasDeclaredWithTypename());
  AddDeclSubstitution(D, RTD);
  SubstitueOrMaintainRequiredDecl(*this, Owner, RTD);

  return RTD;
}

Decl *
InjectionContext::InjectCXXRequiredDeclaratorDecl(CXXRequiredDeclaratorDecl *D) {
  DeclContext *Owner = getSema().CurContext;

  DeclaratorDecl *NewDD = D->getRequiredDeclarator();
  // FIXME: This is _DEFINITELY_ wrong.
  if (hasTemplateArgs() && InstantiateTemplates) {
    // FIXME: Do this eventually
    // SubstQualifier(getSema(), D, NewD, TemplateArgs);
    SemaRef.AnalyzingRequiredDeclarator = true;
    for (const auto &TemplateArgs : getTemplateArgs()) {
      NewDD = cast<DeclaratorDecl>(
          getSema().SubstDecl(NewDD, Owner, TemplateArgs));
    }
    SemaRef.AnalyzingRequiredDeclarator = false;
  }

  CXXRequiredDeclaratorDecl *RDD =
    CXXRequiredDeclaratorDecl::Create(SemaRef.Context, Owner,
                                      NewDD, D->getRequiresLoc());
  AddDeclSubstitution(D, RDD);
  SubstitueOrMaintainRequiredDecl(*this, Owner, RDD);

  return RDD;
}

} // namespace clang

Decl *Sema::ActOnStartCXXStmtFragment(Scope *S, SourceLocation Loc,
                                      bool HasThisPtr) {
  CXXStmtFragmentDecl *StmtFragment = CXXStmtFragmentDecl::Create(
      Context, CurContext, Loc, HasThisPtr);

  PushFunctionScope();
  if (S)
    PushDeclContext(S, StmtFragment);

  return StmtFragment;
}

static void CleanupStmtFragment(Sema &SemaRef, Scope *S) {
  if (S)
    SemaRef.PopDeclContext();

  SemaRef.PopFunctionScopeInfo();
}

void Sema::ActOnCXXStmtFragmentError(Scope *S) {
  CleanupStmtFragment(*this, S);
}

Decl *Sema::ActOnFinishCXXStmtFragment(Scope *S, Decl *StmtFragment,
                                       Stmt *Body) {
  cast<CXXStmtFragmentDecl>(StmtFragment)->setBody(Body);

  CleanupStmtFragment(*this, S);

  return StmtFragment;
}

/// Called at the start of a source code fragment to establish the fragment
/// declaration and placeholders.
CXXFragmentDecl *Sema::ActOnStartCXXFragment(Scope *S, SourceLocation Loc) {
  CXXFragmentDecl *Fragment = CXXFragmentDecl::Create(Context, CurContext, Loc);
  if (S)
    PushDeclContext(S, Fragment);

  return Fragment;
}

/// Binds the content the fragment declaration. Returns the updated fragment.
/// The Fragment is nullptr if an error occurred during parsing. However,
/// we still need to pop the declaration context.
CXXFragmentDecl *Sema::ActOnFinishCXXFragment(
    Scope *S, Decl *Fragment, Decl *Content) {
  CXXFragmentDecl *FD = nullptr;
  if (Fragment) {
    FD = cast<CXXFragmentDecl>(Fragment);
    FD->setContent(Content);
  }

  if (S)
    PopDeclContext();

  return FD;
}

ExprResult Sema::ActOnCXXFragmentCaptureExpr(
    SourceLocation BeginLoc, unsigned Offset, SourceLocation EndLoc) {
  if (!FragmentScope) {
    Diag(BeginLoc, diag::err_invalid_unquote_operator);
    return ExprError();
  }

  ExprResult Result = BuildCXXFragmentCaptureExpr(
      BeginLoc, /*FragmentExpr=*/nullptr, Offset, EndLoc);
  if (Result.isInvalid())
    return ExprError();

  // Get the capture and queue it to be reattached after
  // the expression is parsed. This is purely for printing.
  auto *E = cast<CXXFragmentCaptureExpr>(Result.get());
  assert(!E->getFragment());
  DetachedCaptures.push_back(E);

  return Result;
}

ExprResult Sema::BuildCXXFragmentCaptureExpr(
    SourceLocation BeginLoc, const Expr *FragmentExpr,
    unsigned Offset, SourceLocation EndLoc) {
  return CXXFragmentCaptureExpr::Create(
      Context, FragmentExpr, Offset, BeginLoc, EndLoc);
}

/// This method attaches CXXFragmentCaptureExpr's to their
/// CXXFragmentExpr so that their initializers can be printed prior to
/// the capture being injected.
void Sema::attachCaptures(CXXFragmentExpr *E) {
  assert(FragmentScope && "trying to attach captures too late");
  assert(E->getNumCaptures() == DetachedCaptures.size());

  for (CXXFragmentCaptureExpr *CapExpr : DetachedCaptures) {
    CapExpr->setFragment(E);
  }

  DetachedCaptures.clear();
}

/// Builds a new fragment expression.
ExprResult Sema::ActOnCXXFragmentExpr(SourceLocation Loc, Decl *Fragment,
                                      SmallVectorImpl<Expr *> &Captures) {
  ExprResult Result = BuildCXXFragmentExpr(Loc, Fragment, Captures);
  if (Result.isInvalid())
    return ExprError();

  attachCaptures(cast<CXXFragmentExpr>(Result.get()));

  return Result;
}

/// Builds a new fragment expression.
ExprResult Sema::BuildCXXFragmentExpr(SourceLocation Loc, Decl *Fragment,
                                      SmallVectorImpl<Expr *> &Captures) {
  CXXFragmentDecl *FD = cast<CXXFragmentDecl>(Fragment);
  return CXXFragmentExpr::Create(Context, Loc, FD, Captures);
}

/// Returns true if invalid.
bool
Sema::ActOnCXXSpecifiedNamespaceInjectionContext(SourceLocation BeginLoc,
                                                 Decl *NamespaceDecl,
                                        CXXInjectionContextSpecifier &Specifier,
                                                 SourceLocation EndLoc) {
  Specifier = CXXInjectionContextSpecifier(BeginLoc, NamespaceDecl, EndLoc);
  return false;
}

/// Returns true if invalid.
bool
Sema::ActOnCXXParentNamespaceInjectionContext(SourceLocation KWLoc,
                                      CXXInjectionContextSpecifier &Specifier) {
  Specifier = CXXInjectionContextSpecifier(
                          KWLoc, CXXInjectionContextSpecifier::ParentNamespace);
  return false;
}

/// Returns an injection statement.
StmtResult Sema::ActOnCXXInjectionStmt(SourceLocation Loc,
                           const CXXInjectionContextSpecifier &ContextSpecifier,
                                       Expr *Operand) {
  return BuildCXXInjectionStmt(Loc, ContextSpecifier, Operand);
}

static bool
isTypeOrValueDependent(Expr *FragmentOrReflection) {
  return FragmentOrReflection->isTypeDependent()
      || FragmentOrReflection->isValueDependent();
}

static bool
CheckInjectionOperand(Sema &S, Expr *Operand) {
  QualType Type = Operand->getType();
  if (Type->isFragmentType())
    return true;

  if (Type->isReflectionType())
    return true;

  S.Diag(Operand->getExprLoc(), diag::err_invalid_injection_operand)
    << Type;
  return false;
}

/// Returns an injection statement.
StmtResult Sema::BuildCXXInjectionStmt(SourceLocation Loc,
                           const CXXInjectionContextSpecifier &ContextSpecifier,
                                       Expr *Operand) {
  if (!isConstantEvaluated()) {
    Diag(Loc, diag::err_requires_manifest_constevaluation) << 1;
    return StmtError();
  }

  // If the operand is not dependent, it must be resolveable either
  // to an injectable reflection, or a fragment.
  //
  // An injectable reflection is defined as any reflection which
  // we can resolve to a declaration.
  bool IsDependent = isTypeOrValueDependent(Operand);
  if (!IsDependent && !CheckInjectionOperand(*this, Operand)) {
    return StmtError();
  }

  return new (Context) CXXInjectionStmt(Loc, ContextSpecifier, Operand);
}

StmtResult Sema::ActOnCXXBaseInjectionStmt(
    SourceLocation KWLoc, SourceLocation LParenLoc,
    SmallVectorImpl<CXXBaseSpecifier *> &BaseSpecifiers,
    SourceLocation RParenLoc) {
  return BuildCXXBaseInjectionStmt(KWLoc, LParenLoc, BaseSpecifiers, RParenLoc);
}

StmtResult Sema::BuildCXXBaseInjectionStmt(
    SourceLocation KWLoc, SourceLocation LParenLoc,
    SmallVectorImpl<CXXBaseSpecifier *> &BaseSpecifiers,
    SourceLocation RParenLoc) {
  return CXXBaseInjectionStmt::Create(Context, KWLoc, LParenLoc,
                                      BaseSpecifiers, RParenLoc);
}

static Decl *getFragInjectionDecl(const CXXFragmentExpr *E) {
  return E->getFragment()->getContent();
}

// Returns an integer value describing the target context of the injection.
// This correlates to the second %select in err_invalid_injection.
static int DescribeDeclContext(DeclContext *DC) {
  if (DC->isFunction() || DC->isStatementFragment())
    return 0;
  else if (DC->isMethod() || DC->isMemberStatementFragment())
    return 1;
  else if (DC->isRecord())
    return 2;
  else if (DC->isEnum())
    return 3;
  else if (DC->isFileContext())
    return 4;
  else
    llvm_unreachable("Invalid injection context");
}

struct TypedValue
{
  QualType Type;
  APValue Value;
};

class InjectionCompatibilityChecker {
  Sema &SemaRef;
  SourceLocation POI;
  DeclContext *Injection;
  DeclContext *Injectee;

public:
  InjectionCompatibilityChecker(Sema &SemaRef, SourceLocation POI,
                                DeclContext *Injection, DeclContext *Injectee)
    : SemaRef(SemaRef), POI(POI), Injection(Injection), Injectee(Injectee) { }

  /// Returns true if injection and injectee are two incompatibile
  /// contexts.
  template<typename F>
  bool operator ()(F Test) const {
    bool Failure = Test(Injection) && !Test(Injectee);
    if (Failure) ReportFailure();
    return Failure;
  }

private:
  // Report an error describing what could not be injected into what.
  void ReportFailure() const {
    SemaRef.Diag(POI, diag::err_invalid_injection)
      << DescribeDeclContext(Injection) << DescribeDeclContext(Injectee);
  }
};

static bool CheckInjectionContexts(Sema &SemaRef, SourceLocation POI,
                                   DeclContext *Injection,
                                   DeclContext *Injectee) {
  InjectionCompatibilityChecker Check(SemaRef, POI, Injection, Injectee);

  auto BlockTest = [] (DeclContext *DC) -> bool {
    return DC->isFunction() || DC->isStatementFragment();
  };
  if (Check(BlockTest))
    return false;

  auto MemberBlockTest = [] (DeclContext *DC) -> bool {
    return DC->isMethod() || DC->isMemberStatementFragment();
  };
  if (Check(MemberBlockTest))
    return false;

  auto ClassTest = [] (DeclContext *DC) -> bool {
    return DC->isRecord();
  };
  if (Check(ClassTest))
    return false;

  auto EnumTest = [] (DeclContext *DC) -> bool {
    return DC->isEnum();
  };
  if (Check(EnumTest))
    return false;

  auto NamespaceTest = [] (DeclContext *DC) -> bool {
    return DC->isFileContext();
  };
  if (Check(NamespaceTest))
    return false;

  return true;
}

static InjectionContext *GetLastInjectionContext(Sema &S) {
  if (S.PendingClassMemberInjections.empty())
    return nullptr;

  return S.PendingClassMemberInjections.back();
}


/// This class is responsible for determining what InjectionContext
/// is the current injection context.
///
/// Additionally, before destroying any allocated InjectionContext,
/// this class will check to see if it has any pending injections
/// and update sema state accordingly.
///
/// This is accomplished by checking the viability of any existing
/// injection context, before then creating a new injection context.
class InjectionContextManagerRAII {
  Sema &SemaRef;
  InjectionContext *OriginalContext;
  bool NewContext;
public:
  InjectionContextManagerRAII(Sema &SemaRef, Decl *Injectee)
      : SemaRef(SemaRef), OriginalContext(SemaRef.CurrentInjectionContext) {
    if (tryToUseCurrentContext(Injectee))
      return;

    if (tryToUseLastContext(Injectee))
      return;

    useNewContext(Injectee);
  }
  ~InjectionContextManagerRAII() {
    cleanupContextIfNew();
    restorePreviousContext();
  }

  InjectionContext *getContext() {
    return SemaRef.CurrentInjectionContext;
  }
private:
  bool tryToUseCurrentContext(Decl *Injectee) {
    if (!SemaRef.CurrentInjectionContext)
      return false;

    if (SemaRef.CurrentInjectionContext->Injectee != Injectee)
      return false;

    NewContext = false;

    return true;
  }

  InjectionContext *FindExistingInjectionContext(Decl *Injectee) {
    if (InjectionContext *LastCtx = GetLastInjectionContext(SemaRef)) {
      if (LastCtx->Injectee == Injectee) {
        return LastCtx;
      }
    }

    return nullptr;
  }

  bool tryToUseLastContext(Decl *Injectee) {
    SemaRef.CurrentInjectionContext = FindExistingInjectionContext(Injectee);
    if (!SemaRef.CurrentInjectionContext)
      return false;

    NewContext = false;

    return true;
  }

  void useNewContext(Decl *Injectee) {
    SemaRef.CurrentInjectionContext = new InjectionContext(SemaRef, Injectee);

    NewContext = true;
  }

  void cleanupContextIfNew() {
    if (!NewContext)
      return;

    InjectionContext *Ctx = getContext();
    if (Ctx->hasPendingInjections()) {
      /// This will be cleaned up when PendingClassMemberInjections
      /// are iterated on.
      SemaRef.PendingClassMemberInjections.push_back(Ctx);
      return;
    }

    delete Ctx;
  }

  void restorePreviousContext() {
    SemaRef.CurrentInjectionContext = OriginalContext;
  }
};

template<typename IT, typename F>
static bool BootstrapInjection(Sema &S, Decl *Injectee, IT *Injection,
                               F InjectionProcedure) {
  InjectionContextManagerRAII CtxManager(S, Injectee);

  InjectionContext *Ctx = CtxManager.getContext();
  Ctx->InitInjection(Injection, [&InjectionProcedure, &Ctx] {
    InjectionProcedure(Ctx);
  });

  return !Injectee->isInvalidDecl();
}

static void PerformInjection(InjectionContext *Ctx, Decl *Injectee, Decl *Injection) {
  // The logic for block fragments is different, since everything in the fragment
  // is stored in a CompoundStmt.
  if (isa<CXXStmtFragmentDecl>(Injection)) {
    CXXStmtFragmentDecl *InjectionSFD = cast<CXXStmtFragmentDecl>(Injection);
    CompoundStmt *FragmentBlock = cast<CompoundStmt>(InjectionSFD->getBody());
    for (Stmt *S : FragmentBlock->body()) {
      Stmt *Inj = Ctx->InjectStmt(S);

      if (!Inj) {
        Injectee->setInvalidDecl(true);
        continue;
      }
    }
  } else {
    DeclContext *InjectionAsDC = Decl::castToDeclContext(Injection);
    for (Decl *D : InjectionAsDC->decls()) {
      // Never inject injected class names.
      if (CXXRecordDecl *Class = dyn_cast<CXXRecordDecl>(D)) {
        if (Class->isInjectedClassName())
          continue;
      }

      Decl *R = Ctx->InjectDecl(D);
      if (!R || R->isInvalidDecl()) {
        Injectee->setInvalidDecl(true);
        continue;
      }
    }
  }
}

static void UpdateInjector(InjectionContext *Ctx, CXXInjectorDecl *ID, Decl *Injection) {
  if (isa<CXXStmtFragmentDecl>(Injection)) {
    ID->getInjectedStmts().append(Ctx->InjectedStmts.begin(),
                                  Ctx->InjectedStmts.end());
  } else {
    ID->getInjectedDecls().append(Ctx->InjectedDecls.begin(),
                                  Ctx->InjectedDecls.end());
  }
}

/// Inject a fragment into the current context.
static bool InjectFragment(
    Sema &S, CXXInjectorDecl *ID, const CXXFragmentExpr *FragExpr,
    const Sema::FragInstantiationArgTy &InjectionTemplateArgs,
    const SmallVector<InjectionCapture, 8> &Captures, Decl *Injectee) {
  SourceLocation POI = ID->getSourceRange().getEnd();

  Decl *Injection = getFragInjectionDecl(FragExpr);

  DeclContext *InjectionAsDC = Decl::castToDeclContext(Injection);
  DeclContext *InjecteeAsDC = Decl::castToDeclContext(Injectee);

  if (!CheckInjectionContexts(S, POI, InjectionAsDC, InjecteeAsDC))
    return false;

  Sema::ContextRAII Switch(S, InjecteeAsDC, isa<CXXRecordDecl>(Injectee));

  return BootstrapInjection(S, Injectee, Injection, [&](InjectionContext *Ctx) {
    // Setup substitutions.
    Ctx->AddCapturedValues(FragExpr, Captures);
    Ctx->MaybeAddDeclSubstitution(Injection, Injectee);
    Ctx->AddTemplateArgs(InjectionTemplateArgs);

    // Add source location information for the current injection.
    Ctx->SetPointOfInjection(POI);

    // Legacy setup.
    Ctx->AddLegacyPlaceholderSubstitutions(Injection->getDeclContext(), Captures);

    // Perform the transfer from Injection to Injectee.
    PerformInjection(Ctx, Injectee, Injection);

    // Update the injector with any pending artifacts of this injection.
    UpdateInjector(Ctx, ID, Injection);
  });
}

// Inject a reflected base specifier into the current context.
static bool AddBase(Sema &S, CXXInjectorDecl *MD,
                    CXXBaseSpecifier *Injection,
                    const ReflectionModifiers &Modifiers,
                    Decl *Injectee) {
  SourceLocation POI = MD->getSourceRange().getEnd();
  // DeclContext *InjectionDC = Injection->getDeclContext();
  // Decl *InjectionOwner = Decl::castFromDeclContext(InjectionDC);
  DeclContext *InjecteeAsDC = Decl::castToDeclContext(Injectee);

  // FIXME: Ensure we're injecting into a class.
  // if (!CheckInjectionContexts(S, POI, InjectionDC, InjecteeAsDC))
  //   return false;

  // Establish injectee as the current context.
  Sema::ContextRAII Switch(S, InjecteeAsDC, isa<CXXRecordDecl>(Injectee));

  return BootstrapInjection(S, Injectee, Injection, [&](InjectionContext *Ctx) {
    Ctx->SetModifiers(Modifiers);
    Ctx->SetPointOfInjection(POI);

    // Inject the base specifier.
    if (Ctx->InjectBaseSpecifier(Injection))
      Injectee->setInvalidDecl(true);
  });
}

// Inject a reflected declaration into the current context.
static bool CopyDeclaration(Sema &S, CXXInjectorDecl *MD,
                            Decl *Injection,
                            const ReflectionModifiers &Modifiers,
                            Decl *Injectee) {
  SourceLocation POI = MD->getSourceRange().getEnd();

  DeclContext *InjectionDC = Injection->getDeclContext();
  Decl *InjectionOwner = Decl::castFromDeclContext(InjectionDC);
  DeclContext *InjecteeAsDC = Decl::castToDeclContext(Injectee);

  // Don't copy injected class names.
  if (CXXRecordDecl *Class = dyn_cast<CXXRecordDecl>(Injection))
    if (Class->isInjectedClassName())
      return true;

  if (!CheckInjectionContexts(S, POI, InjectionDC, InjecteeAsDC))
    return false;

  // Establish injectee as the current context.
  Sema::ContextRAII Switch(S, InjecteeAsDC, isa<CXXRecordDecl>(Injectee));

  return BootstrapInjection(S, Injectee, Injection, [&](InjectionContext *Ctx) {
    // Setup substitutions
    Ctx->MaybeAddDeclSubstitution(InjectionOwner, Injectee);
    Ctx->SetModifiers(Modifiers);

    // Add source location information for the current injection.
    Ctx->SetPointOfInjection(POI);

    // Inject the declaration.
    Decl* Result = Ctx->InjectDecl(Injection);
    if (!Result || Result->isInvalidDecl()) {
      Injectee->setInvalidDecl(true);
    }
  });
}

static Decl *
GetInjecteeDecl(Sema &S, DeclContext *CurContext,
                const CXXInjectionContextSpecifier& ContextSpecifier) {
  Decl *CurContextDecl = Decl::castFromDeclContext(CurContext);

  switch (ContextSpecifier.getContextKind()) {
  case CXXInjectionContextSpecifier::CurrentContext:
    return CurContextDecl;

  case CXXInjectionContextSpecifier::ParentNamespace: {
    if (isa<TranslationUnitDecl>(CurContextDecl)) {
      S.Diag(ContextSpecifier.getBeginLoc(),
             diag::err_injecting_into_parent_of_global_namespace);
      return nullptr;
    }

    DeclContext *Parent = Decl::castToDeclContext(CurContextDecl);
    do {
      Parent = Parent->getParent();
    } while (!Parent->isFileContext());

    return Decl::castFromDeclContext(Parent);
  }

  case CXXInjectionContextSpecifier::SpecifiedNamespace:
    return ContextSpecifier.getSpecifiedNamespace();
  }

  llvm_unreachable("Invalid injection context specifier.");
}

static bool isInsideRecord(const DeclContext *DC) {
  do {
    if (DC->isRecord())
      return true;
    DC = DC->getParent();
  } while (DC);
  return false;
}

static bool isInjectingIntoNamespace(const Decl *Injectee) {
  return Decl::castToDeclContext(Injectee)->isFileContext();
}

static CXXInjectionContextSpecifier
GetDelayedNamespaceContext(const CXXInjectionContextSpecifier &CurSpecifier) {
  switch (CurSpecifier.getContextKind()) {
  case CXXInjectionContextSpecifier::CurrentContext:
    llvm_unreachable("injection should not be delayed");

  case CXXInjectionContextSpecifier::ParentNamespace: {
    return CXXInjectionContextSpecifier();
  }

  case CXXInjectionContextSpecifier::SpecifiedNamespace:
    return CurSpecifier;
  }

  llvm_unreachable("Invalid injection context specifier.");
}

static const CXXFragmentExpr *getFragExpr(InjectionEffect &IE) {
  return cast<CXXFragmentExpr>(IE.ExprValue.getFragmentExpr());
}

static Sema::FragInstantiationArgTy
GetTemplateArguments(Sema &S, InjectionEffect &IE) {
  auto &FragmentInstantiationArgs = S.FragmentInstantiationArgs;
  auto Iter = FragmentInstantiationArgs.find(getFragExpr(IE));
  if (Iter != FragmentInstantiationArgs.end())
    return Iter->second;
  else
    return { };
}

static SmallVector<InjectionCapture, 8>
GetFragCaptures(InjectionEffect &IE) {
  // Retrieve state from the InjectionEffect.
  const CXXFragmentExpr *E = getFragExpr(IE);
  const APValue *Values = IE.ExprValue.getFragmentCaptures();
  unsigned NumCaptures = E->getNumCaptures();

  // Allocate space in advanced as we know the size.
  SmallVector<InjectionCapture, 8> Captures;
  Captures.reserve(NumCaptures);

  if (E->isLegacy()) {
    // Map the capture decls to their values.
    for (unsigned I = 0; I < NumCaptures; ++I) {
      auto *DRE = cast<DeclRefExpr>(E->getCapture(I));
      Captures.emplace_back(DRE->getDecl(), Values[I]);
    }
  } else {
    // Map the captures to their values by index.
    for (unsigned I = 0; I < NumCaptures; ++I) {
      Captures.emplace_back(Values[I]);
    }
  }

  return Captures;
}

static bool ApplyFragmentInjection(Sema &S, CXXInjectorDecl *MD,
                                   InjectionEffect &IE, Decl *Injectee) {
  const CXXFragmentExpr *FragExpr = getFragExpr(IE);
  Sema::FragInstantiationArgTy &&TemplateArgs = GetTemplateArguments(S, IE);
  SmallVector<InjectionCapture, 8> &&Captures = GetFragCaptures(IE);
  return InjectFragment(S, MD, FragExpr, TemplateArgs, Captures, Injectee);
}

static Reflection
GetReflectionFromInjection(Sema &S, InjectionEffect &IE) {
  return Reflection(S.Context, IE.ExprValue);
}

static CXXBaseSpecifier *
GetInjectionBase(const Reflection &Refl) {
  const CXXBaseSpecifier *ReachableBase = Refl.getAsBase();

  return const_cast<CXXBaseSpecifier *>(ReachableBase);
}

static const ReflectionModifiers &
GetModifiers(const Reflection &Refl) {
  return Refl.getModifiers();
}

static bool
ApplyReflectionBaseInjection(Sema &S, CXXInjectorDecl *MD,
                             const Reflection &Refl, Decl *Injectee) {
  CXXBaseSpecifier *Injection = GetInjectionBase(Refl);
  ReflectionModifiers Modifiers = GetModifiers(Refl);

  return AddBase(S, MD, Injection, Modifiers, Injectee);
}

static Decl *
GetInjectionDecl(const Reflection &Refl) {
  const Decl *ReachableDecl = Refl.getAsReachableDeclaration();

  // Verify that our reflection contains a reachable declaration.
  assert(ReachableDecl);
  return const_cast<Decl *>(ReachableDecl);
}

static bool
ApplyReflectionDeclInjection(Sema &S, CXXInjectorDecl *MD,
                             const Reflection &Refl, Decl *Injectee) {
  Decl *Injection = GetInjectionDecl(Refl);
  ReflectionModifiers Modifiers = GetModifiers(Refl);

  return CopyDeclaration(S, MD, Injection, Modifiers, Injectee);
}

static bool ApplyReflectionInjection(Sema &S, CXXInjectorDecl *MD,
                                     InjectionEffect &IE, Decl *Injectee) {
  Reflection &&Refl = GetReflectionFromInjection(S, IE);

  if (Refl.isBase())
    return ApplyReflectionBaseInjection(S, MD, Refl, Injectee);

  return ApplyReflectionDeclInjection(S, MD, Refl, Injectee);
}

bool Sema::ApplyInjection(CXXInjectorDecl *MD, InjectionEffect &IE) {
  CodeInjectionTracker InjectingCode(*this);

  Decl *Injectee = GetInjecteeDecl(*this, CurContext, IE.ContextSpecifier);
  if (!Injectee)
    return false;

  // If we're inside of a record, injecting namespace members, delay
  // injection until we finish the class.
  if (isInsideRecord(CurContext) && isInjectingIntoNamespace(Injectee)) {
    // Push a modified injection effect with an adjusted context for replay
    // after completion of the current record.
    auto &&NewSpecifier = GetDelayedNamespaceContext(IE.ContextSpecifier);
    InjectionEffect NewEffect(IE, NewSpecifier);
    PendingNamespaceInjections.push_back({MD, NewEffect});
    return true;
  }

  if (IE.ExprValue.isFragment())
    return ApplyFragmentInjection(*this, MD, IE, Injectee);

  // Type checking should gauarantee that the type of
  // our injection is either a Fragment or reflection.
  // Since we failed the fragment check, we must have
  // a reflection.
  assert(IE.ExprValue.isReflection());

  return ApplyReflectionInjection(*this, MD, IE, Injectee);
}

/// Inject a sequence of source code fragments or modification requests
/// into the current AST. The point of injection (POI) is the point at
/// which the injection is applied.
///
/// returns true if no errors are encountered, false otherwise.
bool Sema::ApplyEffects(CXXInjectorDecl *MD,
                        SmallVectorImpl<InjectionEffect> &Effects) {
  bool Ok = true;
  for (InjectionEffect &Effect : Effects) {
    Ok &= ApplyInjection(MD, Effect);
  }

  return Ok;
}

/// Check if there are any pending definitions of member functions for
/// this class or any of its nested class definitions. We can simply look
/// at the most recent injection; if it's D or declared inside D, then
/// the answer is yes. Otherwise the answer is no.
///
/// We need to check for this whenever a class is completed during an
/// injection. We don't want to prematurely inject definitions.
///
/// FIXME: It's likely that this wouldn't be necessarily if we integrated
/// injection contexts into the template instantiation context; they are
/// somewhat similar.
bool Sema::HasPendingInjections(DeclContext *D) {
  bool IsEmpty = PendingClassMemberInjections.empty();
  if (IsEmpty)
    return false;

  InjectionContext *Ctx = PendingClassMemberInjections.back();

  assert(Ctx->hasPendingInjections() && "bad injection queue");
  DeclContext *DC =  Decl::castToDeclContext(Ctx->Injectee);
  while (!DC->isFileContext()) {
    if (DC == D)
      return true;
    DC = DC->getParent();
  }

  return false;
}

/// Returns true if the DeclContext D both has pending injections
/// and is in a state suiteable for injecting those injections.
bool Sema::ShouldInjectPendingDefinitionsOf(CXXRecordDecl *D) {
  assert(D && "CXXRecordDecl must be specified");

  if (!HasPendingInjections(D))
    return false;

  if (D->isInFragment())
    return false;

  if (D->isCXXClassMember()) {
    auto *ParentClass = cast<CXXRecordDecl>(D->getDeclContext());
    if (!ParentClass->isCompleteDefinition())
      return false;
  }

  return true;
}

class PendingClassInjectionProcessor {
  using CollectionTy = SmallVectorImpl<InjectionContext *>;

  Decl *Injectee;
public:
  PendingClassInjectionProcessor() : Injectee(nullptr) { }

private:
  Decl *getRootClass(InjectionContext *Ctx) {
    CXXRecordDecl *LocalInjectee = cast<CXXRecordDecl>(Ctx->Injectee);

    while (true) {
      DeclContext *DC = LocalInjectee->getDeclContext();
      if (isa<CXXRecordDecl>(DC)) {
        LocalInjectee = cast<CXXRecordDecl>(DC);
        continue;
      }

      break;
    }

    return LocalInjectee;
  }

  /// Checks to see if this injection context is injecting
  /// for the same non-nested class definition as the
  /// injection context we first observed.
  ///
  /// Returns true if we should stop processing due to a
  /// mismatch.
  bool shouldStopProcessing(InjectionContext *Ctx) {
    if (!Injectee) {
      Injectee = getRootClass(Ctx);
      return false;
    }

    return getRootClass(Ctx) != Injectee;
  }

public:
  template<typename F>
  void process(CollectionTy &PendingCtxs, F Op) {
    auto It = PendingCtxs.rbegin();
    auto ItEnd = PendingCtxs.rend();

    while (It != ItEnd) {
      if (shouldStopProcessing(*It))
        break;

      Op(*It);
      ++It;
    }
  }

  void cleanup(CollectionTy &PendingCtxs) {
    while (!PendingCtxs.empty()) {
      InjectionContext *Ctx = PendingCtxs.back();
      if (shouldStopProcessing(Ctx))
        break;

      Ctx->ForEachPendingInjection([&Ctx] {
        Ctx->verifyHasAllRequiredRelatives();
      });

      delete Ctx;
      PendingCtxs.pop_back();
    }
  }
};

void Sema::InjectPendingFieldDefinitions() {
  PendingClassInjectionProcessor Processor;
  Processor.process(PendingClassMemberInjections, [this](InjectionContext *Ctx) {
    InjectPendingFieldDefinitions(Ctx);
  });
}

void Sema::InjectPendingMethodDefinitions() {
  PendingClassInjectionProcessor Processor;
  Processor.process(PendingClassMemberInjections, [this](InjectionContext *Ctx) {
    InjectPendingMethodDefinitions(Ctx);
  });
}

void Sema::InjectPendingFriendFunctionDefinitions() {
  PendingClassInjectionProcessor Processor;
  Processor.process(PendingClassMemberInjections, [this](InjectionContext *Ctx) {
    InjectPendingFriendFunctionDefinitions(Ctx);
  });
  Processor.cleanup(PendingClassMemberInjections);
}

static void InjectPendingDefinition(InjectionContext *Ctx,
                                    FieldDecl *OldField,
                                    FieldDecl *NewField) {
  Sema &S = Ctx->getSema();

  // Switch to the class enclosing the newly injected declaration.
  Sema::ContextRAII ClassCtx(S, NewField->getDeclContext());

  // This is necessary to provide the correct lookup behavior
  // for any injected field with a default initializer using
  // a decl owned by the injectee
  QualType RecordType = S.Context.getRecordType(NewField->getParent());
  S.CXXThisTypeOverride = S.Context.getPointerType(RecordType);

  ExprResult Init = Ctx->TransformExpr(OldField->getInClassInitializer());
  if (Init.isInvalid())
    NewField->setInvalidDecl();
  else
    NewField->setInClassInitializer(Init.get());
}

static void InjectPendingDefinition(InjectionContext *Ctx,
                                    CXXMethodDecl *OldMethod,
                                    CXXMethodDecl *NewMethod) {
  Sema &S = Ctx->getSema();

  S.ActOnStartOfFunctionDef(nullptr, NewMethod);

  Sema::SynthesizedFunctionScope Scope(S, NewMethod);
  Sema::ContextRAII MethodCtx(S, NewMethod);

  StmtResult NewBody = Ctx->TransformStmt(OldMethod->getBody());
  if (NewBody.isInvalid())
    NewMethod->setInvalidDecl();
  else {
    // Declarations without bodies should already be handled by
    // InjectCXXMethodDecl. If we've reached this point and we have a valid,
    // but null, body, something has gone wrong.
    assert(NewBody.get() && "A defined method was injected without its body.");
    NewMethod->setBody(NewBody.get());
  }

  S.ActOnFinishFunctionBody(NewMethod, NewBody.get(), /*IsInstantiation=*/true);

  if (CXXConstructorDecl *OldCtor = dyn_cast<CXXConstructorDecl>(OldMethod)) {
    CXXConstructorDecl *NewCtor = cast<CXXConstructorDecl>(NewMethod);

    SmallVector<CXXCtorInitializer *, 4> NewInitArgs;
    for (CXXCtorInitializer *OldInitializer : OldCtor->inits()) {
      Expr *OldInit = OldInitializer->getInit();
      Expr *NewInit = Ctx->TransformInitializer(OldInit, true).get();

      CXXCtorInitializer *NewInitializer;

      if (OldInitializer->isAnyMemberInitializer()) {
        Decl *OldField = OldInitializer->getMember();
        FieldDecl *NewField
            = cast<FieldDecl>(Ctx->TransformDecl(SourceLocation(), OldField));

        NewInitializer = S.BuildMemberInitializer(
            NewField, NewInit, OldInitializer->getMemberLocation()).get();
      } else if (OldInitializer->isDelegatingInitializer()
                 || OldInitializer->isBaseInitializer()) {
        TypeSourceInfo *OldTSI = OldInitializer->getTypeSourceInfo();
        TypeSourceInfo *NewTSI = Ctx->TransformType(OldTSI);

        CXXRecordDecl *NewClass
            = cast<CXXRecordDecl>(NewMethod->getDeclContext());

        if (OldInitializer->isBaseInitializer())
          NewInitializer = S.BuildBaseInitializer(NewTSI->getType(), NewTSI,
              NewInit, NewClass, /*EllipsisLoc=*/SourceLocation()).get();
        else
          NewInitializer
            = S.BuildDelegatingInitializer(NewTSI, NewInit, NewClass).get();
      } else {
        llvm_unreachable("Unsupported constructor type");
      }

      NewInitArgs.push_back(NewInitializer);
    }

    S.ActOnMemInitializers(NewCtor, /*ColonLoc*/SourceLocation(),
                           NewInitArgs, /*AnyErrors=*/false);
  } else if (isa<CXXDestructorDecl>(OldMethod)) {
    CXXDestructorDecl *NewDtor = cast<CXXDestructorDecl>(NewMethod);

    S.CheckDestructor(NewDtor);
  }
}

static int calculateTemplateDepthAdjustment(CXXMethodDecl *D) {
  int DepthAdjustment = 0;

  DeclContext *DC = D->getDeclContext();
  while (!DC->isFileContext()) {
    ClassTemplateSpecializationDecl *Spec
          = dyn_cast<ClassTemplateSpecializationDecl>(DC);
    if (Spec && !Spec->isClassScopeExplicitSpecialization()) {
      ++DepthAdjustment;
    }

    DC = DC->getParent();
  }

  return DepthAdjustment;
}

static void InjectPendingDefinition(InjectionContext *Ctx,
                                    FunctionTemplateDecl *D,
                                    CXXMethodDecl *NewMethod) {
  Sema &S = Ctx->getSema();

  FunctionDecl *Function = D->getTemplatedDecl();
  const FunctionDecl *PatternDecl = Function;
  if (D->getInstantiatedFromMemberTemplate()) {
    PatternDecl = D->getInstantiatedFromMemberTemplate()->getTemplatedDecl();
  }
  const FunctionDecl *PatternDef = PatternDecl->getDefinition();

  Stmt *Pattern = nullptr;
  if (PatternDef) {
    Pattern = PatternDef->getBody(PatternDef);
    PatternDecl = PatternDef;
    if (PatternDef->willHaveBody())
      PatternDef = nullptr;
  }

  S.ActOnStartOfFunctionDef(nullptr, NewMethod);

  Sema::SynthesizedFunctionScope Scope(S, NewMethod);
  Sema::ContextRAII MethodCtx(S, NewMethod);

  StmtResult NewBody = Pattern;
  if (D->getInstantiatedFromMemberTemplate()) {
    // We need to substitute out the template parameters we know.
    // Then if substitution succeeds, we need to transform the body,
    // which has yet to be touched by injection transform.
    MultiLevelTemplateArgumentList TemplateArgs =
      S.getTemplateInstantiationArgs(Function, nullptr, false, PatternDecl);

    NewBody = S.SubstStmt(Pattern, TemplateArgs);
  }

  if (NewBody.isInvalid()) {
    NewMethod->setInvalidDecl();
  } else {
    assert(Ctx->IncreaseTemplateDepth == 0);
    Ctx->IncreaseTemplateDepth = calculateTemplateDepthAdjustment(NewMethod);
    NewBody = Ctx->TransformStmt(NewBody.get());
    Ctx->IncreaseTemplateDepth = 0;
    if (NewBody.isInvalid()) {
      NewMethod->setInvalidDecl();
    }
  }

  if (!NewBody.isInvalid()) {
    assert(NewBody.get() && "A defined method was injected without its body.");

    NewMethod->setBody(NewBody.get());
  }

  S.ActOnFinishFunctionBody(NewMethod, NewBody.get(), /*IsInstantiation=*/true);
}

static void InjectPendingDefinition(InjectionContext *Ctx,
                                    FunctionDecl *OldFunction,
                                    FunctionDecl *NewFunction) {
  InjectFunctionDefinition(Ctx, OldFunction, NewFunction);
}

template<typename FromDeclType, typename ToDeclType, InjectedDefType DefType>
static void InjectPendingDefinitions(InjectionContext *Ctx,
                                     InjectionInfo *Injection) {
  Ctx->InjectingPendingDefinitions = true;

  for (InjectedDef Def : Injection->InjectedDefinitions) {
    if (Def.Type != DefType)
      continue;

    InjectPendingDefinition(Ctx,
                            static_cast<FromDeclType *>(Def.Fragment),
                            static_cast<ToDeclType *>(Def.Injected));
  }

  Ctx->InjectingPendingDefinitions = false;
}

template<typename DeclType, InjectedDefType DefType>
static void InjectPendingDefinitions(InjectionContext *Ctx,
                                     InjectionInfo *Injection) {
  InjectPendingDefinitions<DeclType, DeclType, DefType>(Ctx, Injection);
}


template<typename FromDeclType, typename ToDeclType, InjectedDefType DefType>
static void InjectAllPendingDefinitions(InjectionContext *Ctx) {
  Sema::CodeInjectionTracker InjectingCode(Ctx->getSema());

  Ctx->ForEachPendingInjection([&Ctx] {
    InjectPendingDefinitions<FromDeclType, ToDeclType, DefType>(Ctx, Ctx->CurInjection);
  });
}

template<typename DeclType, InjectedDefType DefType>
static void InjectAllPendingDefinitions(InjectionContext *Ctx) {
  InjectAllPendingDefinitions<DeclType, DeclType, DefType>(Ctx);
}

void Sema::InjectPendingFieldDefinitions(InjectionContext *Ctx) {
  InjectAllPendingDefinitions<FieldDecl, InjectedDef_Field>(Ctx);
}

void Sema::InjectPendingMethodDefinitions(InjectionContext *Ctx) {
  InjectAllPendingDefinitions<FunctionTemplateDecl, CXXMethodDecl,
                              InjectedDef_TemplatedMethod>(Ctx);
  InjectAllPendingDefinitions<CXXMethodDecl, InjectedDef_Method>(Ctx);
}

void Sema::InjectPendingFriendFunctionDefinitions(InjectionContext *Ctx) {
  InjectAllPendingDefinitions<FunctionDecl, InjectedDef_FriendFunction>(Ctx);
}

bool Sema::InjectPendingNamespaceInjections() {
  bool Ok = true;

  while (!PendingNamespaceInjections.empty()) {
    auto &&PendingEffect = PendingNamespaceInjections.pop_back_val();
    Ok &= ApplyInjection(PendingEffect.MD, PendingEffect.Effect);
  }

  return Ok;
}

template <typename MetaType>
static MetaType *
ActOnMetaDecl(Sema &Sema, SourceLocation ConstevalLoc) {
  Preprocessor &PP = Sema.PP;
  ASTContext &Context = Sema.Context;

  MetaType *MD;

  // Build the function
  //
  //  constexpr void __constexpr_decl() compound-statement
  //
  // where compound-statement is the as-of-yet parsed body of the
  // constexpr-declaration.
  IdentifierInfo *II = &PP.getIdentifierTable().get("__constexpr_decl");
  DeclarationName Name(II);
  DeclarationNameInfo NameInfo(Name, ConstevalLoc);

  FunctionProtoType::ExtProtoInfo EPI(
      Context.getDefaultCallingConvention(/*IsVariadic=*/false,
                                          /*IsCXXMethod=*/false));
  QualType FunctionTy = Context.getFunctionType(Context.VoidTy, None, EPI);
  TypeSourceInfo *FunctionTyInfo =
      Context.getTrivialTypeSourceInfo(FunctionTy);

  DeclContext *&CurContext = Sema.CurContext;
  FunctionDecl *Function =
      FunctionDecl::Create(Context, CurContext, ConstevalLoc, NameInfo,
                           FunctionTy, FunctionTyInfo, SC_None,
                           /*isInlineSpecified=*/false,
                           /*hasWrittenPrototype=*/true,
                           /*ConstexprKind=*/CSK_consteval,
                           /*TrailingRequiresClause=*/nullptr);
  Function->setImplicit();
  Function->setMetaprogram();

  // Build the meta declaration around the function.
  MD = MetaType::Create(Context, CurContext, ConstevalLoc, Function);

  Sema.ActOnStartOfFunctionDef(nullptr, Function);

  CurContext->addDecl(MD);

  return MD;
}
/// Create a metaprogram-declaration that will hold the body of the
/// metaprogram-declaration.
Decl *Sema::ActOnCXXMetaprogramDecl(SourceLocation ConstevalLoc) {
  return ActOnMetaDecl<CXXMetaprogramDecl>(*this, ConstevalLoc);
}

/// Create a injection-declaration that will hold the body of the
/// injection-declaration.
Decl *Sema::ActOnCXXInjectionDecl(SourceLocation ConstevalLoc) {
  return ActOnMetaDecl<CXXInjectionDecl>(*this, ConstevalLoc);
}

template <typename MetaType>
static void
ActOnStartMetaDecl(Sema &Sema, Decl *D, DeclContext *&OriginalDC) {
  MetaType *MD = cast<MetaType>(D);

  OriginalDC = Sema.CurContext;
  Sema.CurContext = MD->getFunctionDecl();
}

/// Called just prior to parsing the body of a metaprogram-declaration.
///
/// This ensures that the declaration context is changed correctly.
void Sema::ActOnStartCXXMetaprogramDecl(Decl *D, DeclContext *&OriginalDC) {
  ActOnStartMetaDecl<CXXMetaprogramDecl>(*this, D, OriginalDC);
}

/// Called just prior to parsing the body of a injection-declaration.
///
/// This ensures that the declaration context is changed correctly.
void Sema::ActOnStartCXXInjectionDecl(Decl *D, DeclContext *&OriginalDC) {
  ActOnStartMetaDecl<CXXInjectionDecl>(*this, D, OriginalDC);
}

/// Evaluates a call expression for a metaprogram declaration.
///
/// \returns  \c true if the expression \p E can be evaluated, \c false
///           otherwise.
///
template <typename MetaType>
static bool
EvaluateMetaDeclCall(Sema &Sema, MetaType *MD, CallExpr *Call) {
  const LangOptions &LangOpts = Sema.LangOpts;
  ASTContext &Context = Sema.Context;

  // Associate the call expression with the declaration.
  MD->setCallExpr(Call);

  SmallVector<PartialDiagnosticAt, 8> Notes;
  SmallVector<InjectionEffect, 16> Effects;
  Expr::EvalResult Result;
  Result.Diag = &Notes;
  Result.InjectionEffects = &Effects;

  EnterExpressionEvaluationContext ConstantEvaluated(
      Sema, Sema::ExpressionEvaluationContext::ConstantEvaluated);

  Expr::EvalContext EvalCtx(Context, Sema.GetReflectionCallbackObj());
  bool Folded = Call->EvaluateAsRValue(Result, EvalCtx);
  if (!Folded) {
    // If the only error is that we didn't initialize a (void) value, that's
    // actually okay. APValue doesn't know how to do this anyway.
    //
    // FIXME: We should probably have a top-level EvaluateAsVoid() function that
    // handles this case.
    if (!Notes.empty()) {
      // If we got a compiler error, then just emit that.
      if (Notes[0].second.getDiagID() == diag::err_user_defined_error)
        Sema.Diag(MD->getBeginLoc(), Notes[0].second);
      else {
        // FIXME: These source locations are wrong.
        Sema.Diag(MD->getBeginLoc(), diag::err_expr_not_ice) << LangOpts.CPlusPlus;
        for (const PartialDiagnosticAt &Note : Notes)
          Sema.Diag(Note.first, Note.second);
      }
    }
  }

  // Apply any modifications
  Sema.ApplyEffects(MD, Effects);

  return Notes.empty();
}

/// Process a metaprogram-declaration.
///
/// This handles the construction and evaluation
/// -- via its call to EvaluateMetaDeclCall -- of the call expression used
/// for metaprogram declarations represented as a function.
template <typename MetaType>
static bool
EvaluateMetaDecl(Sema &Sema, MetaType *MD, FunctionDecl *D) {
  ASTContext &Context = Sema.Context;

  QualType FunctionTy = D->getType();
  DeclRefExpr *Ref = new (Context) DeclRefExpr(
      Context, D, /*RefersToEnclosingVariableOrCapture=*/false, FunctionTy,
      VK_LValue, MD->getLocation());

  QualType PtrTy = Context.getPointerType(FunctionTy);
  ImplicitCastExpr *Cast = ImplicitCastExpr::Create(
      Context, PtrTy, CK_FunctionToPointerDecay, Ref, /*BasePath=*/nullptr,
      VK_RValue);

  CallExpr *Call = CallExpr::Create(
      Context, Cast, ArrayRef<Expr *>(), Context.VoidTy, VK_RValue,
      MD->getLocation(), Sema.CurFPFeatureOverrides());

  return EvaluateMetaDeclCall(Sema, MD, Call);
}

/// Hook to be called by template instantiation.
void Sema::EvaluateCXXMetaDecl(CXXMetaprogramDecl *D, FunctionDecl *FD) {
  EvaluateMetaDecl(*this, D, FD);
}

/// Hook to be called by template instantiation.
void Sema::EvaluateCXXMetaDecl(CXXInjectionDecl *D, FunctionDecl *FD) {
  EvaluateMetaDecl(*this, D, FD);
}

template <typename MetaType>
static void
ActOnFinishMetaDecl(Sema &Sema, Decl *D, Stmt *Body,
                    DeclContext *OriginalDC, bool FromParser) {
  MetaType *MD = cast<MetaType>(D);
  FunctionDecl *Fn = MD->getFunctionDecl();

  Sema.DiscardCleanupsInEvaluationContext();
  Sema.ActOnFinishFunctionBody(Fn, Body);

  Sema.CurContext = OriginalDC;

  if (!Sema.CurContext->isDependentContext()) {
    bool WasFromParser = Sema.EvaluatingMetaDeclFromParser;

    Sema.EvaluatingMetaDeclFromParser = FromParser;
    EvaluateMetaDecl(Sema, MD, Fn);
    Sema.EvaluatingMetaDeclFromParser = WasFromParser;
  }
}

/// Called immediately after parsing the body of a metaprorgam-declaration.
///
/// The statements within the body are evaluated here.
///
/// This function additionally ensures that the
/// declaration context is restored correctly.
void Sema::ActOnFinishCXXMetaprogramDecl(Decl *D, Stmt *Body,
                                         DeclContext *OriginalDC,
                                         bool FromParser) {
  ActOnFinishMetaDecl<CXXMetaprogramDecl>(*this, D, Body,
                                          OriginalDC, FromParser);
}

/// Called immediately after parsing the body of a injection-declaration.
///
/// The statements within the body are evaluated here.
///
/// This function additionally ensures that the
/// declaration context is restored correctly.
void Sema::ActOnFinishCXXInjectionDecl(Decl *D, Stmt *InjectionStmt,
                                       DeclContext *OriginalDC,
                                       bool FromParser) {
  CompoundStmt *Body = CompoundStmt::Create(Context,
                                            ArrayRef<Stmt *>(InjectionStmt),
                                            SourceLocation(), SourceLocation());
  ActOnFinishMetaDecl<CXXInjectionDecl>(*this, D, Body,
                                        OriginalDC, FromParser);
}

template <typename MetaType>
static void
ActOnCXXMetaError(Sema &Sema, Decl *D, DeclContext *OriginalDC) {
  MetaType *MD = cast<MetaType>(D);
  MD->setInvalidDecl();

  FunctionDecl *Fn = MD->getFunctionDecl();

  Sema.DiscardCleanupsInEvaluationContext();
  Sema.ActOnFinishFunctionBody(Fn, nullptr);

  Sema.CurContext = OriginalDC;
}

/// Called when an error occurs while parsing the metaprogram-declaration body.
///
/// This ensures that the declaration context is restored correctly.
void Sema::ActOnCXXMetaprogramDeclError(Decl *D, DeclContext *OriginalDC) {
  ActOnCXXMetaError<CXXMetaprogramDecl>(*this, D, OriginalDC);
}

/// Called when an error occurs while parsing the injection-declaration body.
///
/// This ensures that the declaration context is restored correctly.
void Sema::ActOnCXXInjectionDeclError(Decl *D, DeclContext *OriginalDC) {
  ActOnCXXMetaError<CXXInjectionDecl>(*this, D, OriginalDC);
}

bool Sema::ActOnCXXInjectedParameter(SourceLocation ArrowLoc, Expr *Reflection,
                           SmallVectorImpl<DeclaratorChunk::ParamInfo> &Parms) {
  CXXInjectedParmsInfo ParmInjectionInfo(ArrowLoc, Reflection);
  ParmVarDecl *New = ParmVarDecl::Create(Context, ParmInjectionInfo);

  New->setScopeInfo(CurScope->getFunctionPrototypeDepth(),
                    CurScope->getNextFunctionPrototypeIndex());

  Parms.push_back(DeclaratorChunk::ParamInfo(nullptr, ArrowLoc, New));
  return true;
}

/// Given an input like this:
///
///    class(metafn) Proto { ... };
///
/// Generate something that looks (about) like this:
///
///    namespace __fake__ { class Proto { ... } };
///    class Class {
///      using prototype = __fake__::Proto;
///      constexpr { metafn(reflexpr(prototype)); }
///    }
///
/// We don't actually need to emit the fake namespace; we just don't
/// add it to a declaration context.
CXXRecordDecl *Sema::ActOnStartMetaclass(CXXRecordDecl *Class,
                                         Expr *Metafunction, TagUseKind TUK) {
  Class->setMetafunction(Metafunction);

  if (TUK == TUK_Definition) {
    // Start defining the (final) class.
    Class->setLexicalDeclContext(CurContext);
    CurContext->addDecl(Class);
    StartDefinition(Class);

    // Create a new, nested class to hold the parsed member.
    DeclarationNameInfo DNI(Class->getDeclName(), Class->getLocation());
    CXXRecordDecl *Proto = CXXRecordDecl::Create(
        Context, Class->getTagKind(), Class, Class->getBeginLoc(),
        Class->getLocation(), DNI.getName().getAsIdentifierInfo(), nullptr);

    return Proto;
  }

  return Class;
}

void Sema::ActOnStartMetaclassDefinition(CXXRecordDecl *Proto) {
  // The prototype must be a fragment in order to suppress
  // default generation of members.
  Proto->setImplicit(true);
  Proto->setFragment(true);
}

/// We've just finished parsing the definition of something like this:
///
///    class(M) C { ... };
///
/// And have conceptually transformed that into something like this.
///
///    namespace __fake__ { class C { ... } };
///
/// Now, we need to build a new version of the class containing a
/// prototype and its generator.
///
///    class C {
///      using prototype = __fake__::C;
///      constexpr { M(reflexpr(prototype)); }
///    };
///
/// The only remaining step is to build and apply the metaprogram to
/// generate the enclosing class.
CXXRecordDecl *Sema::ActOnFinishMetaclass(CXXRecordDecl *Proto, Scope *S,
                                          SourceRange BraceRange) {
  CXXRecordDecl *Class = cast<CXXRecordDecl>(Proto->getDeclContext());
  Expr *Metafunction = Class->getMetafunction();
  assert(Metafunction && "expected metaclass");

  // FIXME: Are there any properties that Class should inherit from
  // the prototype? Alignment and layout attributes?

  // Propagate access level
  Class->setAccess(Proto->getAccess());

  // Make sure that the final class is available in its declaring scope.
  bool IsAnonymousClass = Class->getName().empty();
  if (!IsAnonymousClass)
    PushOnScopeChains(Class, CurScope->getParent(), false);

  // Make the new class is the current declaration context for the
  // purpose of injecting source code.
  ContextRAII Switch(*this, Class);

  // For the purpose of creating the metaprogram and performing
  // the final analysis, the Class needs to be scope's entity, not
  // prototype.
  S->setEntity(Class);

  // Use the name of the class for most source locations.
  //
  // FIXME: This isn't a particularly good idea.
  SourceLocation Loc = Proto->getLocation();

  // Add 'constexpr { M(reflexpr(prototype)); }' to the class.
  Decl *CD = ActOnCXXMetaprogramDecl(Loc);
  CD->setImplicit(true);
  CD->setAccess(AS_public);

  DeclContext *OriginalDC;
  ActOnStartCXXMetaprogramDecl(CD, OriginalDC);

  // Build the expression reflexpr(prototype).
  // This technically is performing the equivalent
  // addition of 'constexpr { M(reflexpr(__fake__::C)); }'.
  QualType ProtoTy = Context.getRecordType(Proto);
  ExprResult Input = BuildCXXReflectExpr(/*Loc=*/SourceLocation(),
                                         ProtoTy,
                                         /*LP=*/SourceLocation(),
                                         /*RP=*/SourceLocation());

  // Build the call to <gen>(<ref>)
  Expr *InputExpr = Input.get();
  MultiExprArg Args(InputExpr);
  ExprResult Call = ActOnCallExpr(CurScope, Metafunction, Loc, Args, Loc);
  if (Call.isInvalid()) {
    ActOnCXXMetaprogramDeclError(CD, OriginalDC);
    Class->setInvalidDecl(true);
  } else {
    Stmt* Body = CompoundStmt::Create(Context, Call.get(), Loc, Loc);
    ActOnFinishCXXMetaprogramDecl(CD, Body, OriginalDC);

    // Finally, re-analyze the fields of the fields the class to
    // instantiate remaining defaults. This will also complete the
    // definition.
    SmallVector<Decl *, 32> Fields;
    ActOnFields(S, Class->getLocation(), Class, Fields,
                BraceRange.getBegin(), BraceRange.getEnd(),
                ParsedAttributesView());
    CheckCompletedCXXClass(nullptr, Class);

    ActOnFinishCXXNonNestedClass(Class);

    assert(Class->isCompleteDefinition() && "Generated class not complete");
  }

  return Class;
}

static bool CheckTypeTransformerOperand(Sema &S, Expr *Operand) {
  QualType OperandType = Operand->getType();

  if (!OperandType->isReflectionType()) {
    S.Diag(Operand->getExprLoc(), diag::err_invalid_type_transformer_operand)
      << OperandType;
    return true;
  }

  return false;
}

Sema::DeclGroupPtrTy Sema::ActOnCXXTypeTransformerDecl(SourceLocation UsingLoc,
                                                       bool IsClass,
                                                       SourceLocation IdLoc,
                                                       IdentifierInfo *Id,
                                                       Expr *Generator,
                                                       Expr *Reflection) {
  // Create the generated type.
  TagTypeKind TTK = IsClass ? TTK_Class : TTK_Struct;
  CXXRecordDecl *Class = CXXRecordDecl::Create(Context, TTK, CurContext,
                                               IdLoc, IdLoc, Id);
  Class->setImplicit(true);

  if (const CXXRecordDecl *RD = dyn_cast<CXXRecordDecl>(CurContext))
    Class->setAccess(RD->getDefaultAccessSpecifier());

  CurContext->addDecl(Class);

  // Check the type, and if invalid, abort before completing the class.
  if (CheckTypeTransformerOperand(*this, Reflection)) {
    Class->setInvalidDecl();
    return DeclGroupPtrTy::make(DeclGroupRef(Class));
  }

  StartDefinition(Class);

  ContextRAII ClassContext(*this, Class);
  // FIXME: If the reflection (ref) is a fragment DO NOT insert the
  // prototype. A fragment is NOT a type.`

  // Insert 'consteval { <gen>(<ref>); }'.
  Decl *CD = ActOnCXXMetaprogramDecl(UsingLoc);
  CD->setImplicit(true);
  CD->setAccess(AS_public);

  DeclContext *OriginalDC;
  ActOnStartCXXMetaprogramDecl(CD, OriginalDC);

  // Build the call to <gen>(<ref>)
  Expr *Args[] { Reflection };
  ExprResult Call = ActOnCallExpr(nullptr, Generator, IdLoc, Args, IdLoc);
  if (Call.isInvalid()) {
    ActOnCXXMetaprogramDeclError(CD, OriginalDC);
    Class->setInvalidDecl(true);
    CompleteDefinition(Class);
    PopDeclContext();
  }

  Stmt *Body = CompoundStmt::Create(Context, Call.get(), IdLoc, IdLoc);
  ActOnFinishCXXMetaprogramDecl(CD, Body, OriginalDC);

  CompleteDefinition(Class);
  PopDeclContext();

  return DeclGroupPtrTy::make(DeclGroupRef(Class));
}

Decl *Sema::ActOnCXXRequiredTypeDecl(AccessSpecifier AS,
                                     SourceLocation RequiresLoc,
                                     SourceLocation TypenameLoc,
                                     IdentifierInfo *Id, bool Typename) {
  CXXRequiredTypeDecl *RTD =
    CXXRequiredTypeDecl::Create(Context, CurContext,
                                RequiresLoc, TypenameLoc, Id, Typename);
  RTD->setAccess(AS);

  PushOnScopeChains(RTD, getCurScope());
  return RTD;
}

Decl *Sema::ActOnCXXRequiredDeclaratorDecl(Scope *CurScope,
                                           SourceLocation RequiresLoc,
                                           Declarator &D) {
  // We don't want to check for linkage, memoize that we're
  // working on a required declarator for later checks.
  AnalyzingRequiredDeclarator = true;
  DeclaratorDecl *DDecl
    = cast<DeclaratorDecl>(ActOnDeclarator(CurScope, D));
  AnalyzingRequiredDeclarator = false;

  if (!DDecl)
    return nullptr;

  // We'll deal with auto deduction later.
  if (ParsingInitForAutoVars.count(DDecl)) {
    ParsingInitForAutoVars.erase(DDecl);

    // Since we haven't deduced the auto type, we will run
    // into problems if the user actually tries to use this
    // declarator. Make it a dependent deduced auto type.
    QualType Sub = SubstAutoType(DDecl->getType(), Context.DependentTy);
    DDecl->setType(Sub);
  }

  CXXRequiredDeclaratorDecl *RDD =
    CXXRequiredDeclaratorDecl::Create(Context, CurContext, DDecl, RequiresLoc);
  return RDD;
}
