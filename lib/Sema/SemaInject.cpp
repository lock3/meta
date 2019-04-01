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
#include "clang/Sema/Initialization.h"
#include "clang/Sema/Template.h"
#include "clang/Sema/SemaInternal.h"

using namespace clang;

namespace clang {
  enum InjectedDefType : unsigned;
  struct InjectionInfo;
  class InjectionContext;
}

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
  InjectedDef_Method
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

struct InjectionCapture {
  FieldDecl *Decl;
  APValue Value;

  InjectionCapture(FieldDecl *Decl, APValue Value)
    : Decl(Decl), Value(Value) { }
};

using InjectionType = llvm::PointerUnion<Decl *, CXXBaseSpecifier *>;

struct InjectionInfo {
  InjectionInfo(InjectionType Injection)
    : Injection(Injection), Modifiers() { }


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

  /// The modifiers to apply to injection.
  ReflectionModifiers Modifiers;

  /// True if we've injected a field. If we have, this injection context
  /// must be preserved until we've finished rebuilding all injected
  /// constructors.
  bool InjectedFieldData = false;

  /// A list of declarations whose definitions have not yet been
  /// injected. These are processed when a class receiving injections is
  /// completed.
  llvm::SmallVector<InjectedDef, 8> InjectedDefinitions;
};

/// An injection context. This is declared to establish a set of
/// substitutions during an injection.
class InjectionContext : public TreeTransform<InjectionContext> {
  using Base = TreeTransform<InjectionContext>;

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

  template<typename IT, typename F>
  void InitInjection(IT Injection, F Op) {
    InjectionInfo *PreviousInjection = CurInjection;
    CurInjection = new InjectionInfo(Injection);

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

    CurInjection = PreviousInjection;
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

  /// Adds a substitution from one declaration to another.
  void AddDeclSubstitution(const Decl *Old, Decl *New) {
    assert(TransformedLocalDecls.count(Old) == 0 && "Overwriting substitution");
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

  /// Adds a substitution from a fragment placeholder to its
  /// (type) constant value.
  void MaybeAddPlaceholderSubstitution(Decl *Orig, QualType T,
                                       const APValue &V) {
    assert(isa<VarDecl>(Orig) && "Expected a variable declaration");
    auto RIter = PlaceholderSubsts.find(Orig);
    if (RIter != PlaceholderSubsts.end()) {
      // FIXME: This doesn't fully check for equivalence of the APValue.
      assert((RIter->second.Type == T
              && RIter->second.Value.getKind() == V.getKind())
             && "Overwriting substitution");
      return;
    }
    PlaceholderSubsts.try_emplace(Orig, T, V);
  }

  /// Adds substitutions for each placeholder in the fragment.
  /// The types and values are sourced from the fields of the reflection
  /// class and the captured values.
  void AddPlaceholderSubstitutions(const DeclContext *Fragment,
                                   const ArrayRef<InjectionCapture> &Captures) {
    assert(isa<CXXFragmentDecl>(Fragment) && "Context is not a fragment");

    auto PlaceIter = Fragment->decls_begin();
    auto PlaceIterEnd = Fragment->decls_end();
    auto CaptureIter = Captures.begin();
    auto CaptureIterEnd = Captures.end();

    while (PlaceIter != PlaceIterEnd && CaptureIter != CaptureIterEnd) {
      Decl *Var = *PlaceIter++;

      const InjectionCapture &IC = (*CaptureIter++);
      QualType Ty = IC.Decl->getType();
      APValue Val = IC.Value;

      MaybeAddPlaceholderSubstitution(Var, Ty, Val);
    }
  }

  bool ShouldInjectInto(DeclContext *DC) const {
    // We should only be creating children of the declaration
    // being injected, if the target DC is the context we're
    // mock injecting into, it should be blocked.
    return DC != MockInjectionContext;
  }

  /// Returns a replacement for D if a substitution has been registered or
  /// nullptr if no such replacement exists.
  Decl *GetDeclReplacement(Decl *D) {
    auto Iter = TransformedLocalDecls.find(D);
    if (Iter != TransformedLocalDecls.end())
      return Iter->second;
    else
      return nullptr;
  }

  /// Returns a replacement expression if E refers to a placeholder.
  Expr *GetPlaceholderReplacement(DeclRefExpr *E) {
    auto Iter = PlaceholderSubsts.find(E->getDecl());
    if (Iter != PlaceholderSubsts.end()) {
      // Build a new constant expression as the replacement. The source
      // expression is opaque since the actual declaration isn't part of
      // the output AST (but we might want it as context later -- makes
      // pretty printing more elegant).
      const TypedValue &TV = Iter->second;
      Expr *Opaque = new (getContext()) OpaqueValueExpr(
          E->getLocation(), TV.Type, VK_RValue, OK_Ordinary, E);
      return new (getContext()) CXXConstantExpr(Opaque, TV.Value);
    } else {
      return nullptr;
    }
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
    if (DeclContext *DC = getInjectionDeclContext())
      return isa<CXXFragmentDecl>(DC);
    return false;
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
      if (D->getDeclContext() == getInjectionDeclContext())
        return nullptr;
    }

    return D;
  }

  Decl *TransformDefinition(SourceLocation Loc, Decl *D) {
    // Rebuild the by injecting it. This will apply substitutions to the type
    // and initializer of the declaration.
    return InjectDecl(D);
  }

  bool TransformCXXFragmentContent(CXXFragmentDecl *NewFrag,
                                   Decl *OriginalContent,
                                   Decl *&NewContent) {
    // Change the target of injection, by temporarily changing
    // the current context. This is required for the MockInjectDecl
    // call to properly determine what should and shouldn't be injected.
    Sema::ContextRAII Context(SemaRef, Decl::castToDeclContext(NewFrag));

    NewContent = MockInjectDecl(OriginalContent);

    return !NewContent;
  }

  ExprResult TransformDeclRefExpr(DeclRefExpr *E) {
    if (Expr *R = GetPlaceholderReplacement(E)) {
      return R;
    }

    return Base::TransformDeclRefExpr(E);
  }

  bool ExpandInjectedParameter(const CXXInjectedParmsInfo &Injected,
                               SmallVectorImpl<ParmVarDecl *> &Parms) {
    ExprResult TransformedOperand = getDerived().TransformExpr(
                                                              Injected.Operand);
    if (TransformedOperand.isInvalid())
      return true;

    Expr *Operand = TransformedOperand.get();
    Sema::ExpansionContextBuilder CtxBldr(SemaRef, SemaRef.getCurScope(),
                                          Operand);
    if (CtxBldr.BuildCalls())
      ; // TODO: Diag << failed to build calls

    // Traverse the range now and add the exprs to the vector
    Sema::RangeTraverser Traverser(SemaRef, CtxBldr.getKind(),
                                   CtxBldr.getRangeBeginCall(),
                                   CtxBldr.getRangeEndCall());

    while (!Traverser) {
      Reflection R = EvaluateReflection(SemaRef, *Traverser);
      Decl *ReflectD = const_cast<Decl *>(R.getAsDeclaration());
      Parms.push_back(cast<ParmVarDecl>(ReflectD));

      ++Traverser;
    }

    return false;
  }

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
      if (const CXXInjectedParmsInfo *Injected = Parm->InjectedParmsInfo) {
        SmallVector<ParmVarDecl *, 4> ExpandedParms;
        if (ExpandInjectedParameter(*Injected, ExpandedParms))
          return true;

        // Add the new Params.
        Params->append(ExpandedParms.begin(), ExpandedParms.end());

        // Add the substitition.
        InjectedParms[Parm] = ExpandedParms;
      } else {
        Params->push_back(Parm);
      }
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
  Decl *InjectCXXMethodDecl(CXXMethodDecl *D);
  Decl *InjectDeclImpl(Decl *D);
  Decl *InjectDecl(Decl *D);
  Decl *MockInjectDecl(Decl *D);
  Decl *InjectAccessSpecDecl(AccessSpecDecl *D);
  Decl *InjectCXXMetaprogramDecl(CXXMetaprogramDecl *D);
  Decl *InjectCXXInjectionDecl(CXXInjectionDecl *D);

  TemplateParameterList *InjectTemplateParms(TemplateParameterList *Old);
  Decl *InjectClassTemplateDecl(ClassTemplateDecl *D);
  Decl *InjectClassTemplateSpecializationDecl(ClassTemplateSpecializationDecl *D);
  Decl *InjectFunctionTemplateDecl(FunctionTemplateDecl *D);
  Decl *InjectTemplateTypeParmDecl(TemplateTypeParmDecl *D);
  Decl *InjectStaticAssertDecl(StaticAssertDecl *D);

  // Members

  /// A mapping of fragment placeholders to their typed compile-time
  /// values. This is used by TreeTransformer to replace references with
  /// constant expressions.
  llvm::DenseMap<Decl *, TypedValue> PlaceholderSubsts;

  /// A mapping of injected parameters to their corresponding
  /// expansions.
  llvm::DenseMap<ParmVarDecl *, SmallVector<ParmVarDecl *, 4>> InjectedParms;

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

  /// The DeclContext we're mock injecting for.
  DeclContext *MockInjectionContext = nullptr;

  /// The context into which the fragment is injected
  Decl *Injectee;

  /// The current injection being injected.
  InjectionInfo *CurInjection = nullptr;

  /// The pending class member injections.
  llvm::SmallVector<InjectionInfo *, 8> PendingInjections;
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
  if (OldParms.size() > 0) {
    do {
      ParmVarDecl *OldParm = OldParms[OldIndex++];
      if (auto *Injected = FindInjectedParms(OldParm)) {
        for (unsigned I = 0; I < Injected->size(); ++I) {
          ParmVarDecl *NewParm = NewParms[NewIndex++];
          UpdateFunctionParm(*this, New, OldParm, NewParm);
        }
      } else {
        ParmVarDecl *NewParm = NewParms[NewIndex++];
        UpdateFunctionParm(*this, New, OldParm, NewParm);

        AddDeclSubstitution(OldParm, NewParm);
      }
    } while (OldIndex < OldParms.size() && NewIndex < NewParms.size());
  } else {
    assert(NewParms.size() == 0);
  }
  assert(OldIndex == OldParms.size() && NewIndex == NewParms.size());
}

Decl* InjectionContext::InjectNamespaceDecl(NamespaceDecl *D) {
  DeclContext *Owner = getSema().CurContext;

  // Build the namespace.
  //
  // FIXME: Search for a previous declaration of the namespace so that they
  // can be stitched together (i.e., redo lookup).
  NamespaceDecl *Ns = NamespaceDecl::Create(
      getContext(), Owner, D->isInline(), D->getLocation(), D->getLocation(),
      D->getIdentifier(), /*PrevDecl=*/nullptr);
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

Decl *InjectionContext::InjectFunctionDecl(FunctionDecl *D) {
  DeclContext *Owner = getSema().CurContext;

  DeclarationNameInfo DNI;
  TypeSourceInfo* TSI;
  bool Invalid = InjectDeclarator(D, DNI, TSI);

  // FIXME: Check for redeclaration.

  FunctionDecl* Fn = FunctionDecl::Create(
      getContext(), Owner, D->getLocation(), DNI, TSI->getType(), TSI,
      D->getStorageClass(), D->isInlineSpecified(), D->hasWrittenPrototype(),
      D->isConstexpr());
  AddDeclSubstitution(D, Fn);
  UpdateFunctionParms(D, Fn);

  // Update the constexpr specifier.
  if (GetModifiers().addConstexpr()) {
    Fn->setConstexpr(true);
    Fn->setType(Fn->getType().withConst());
  } else {
    Fn->setConstexpr(D->isConstexpr());
  }

  // Set properties.
  Fn->setInlineSpecified(D->isInlineSpecified());
  Fn->setInvalidDecl(Invalid);

  // Don't register the declaration if we're merely attempting to transform
  // this class.
  if (ShouldInjectInto(Owner))
    Owner->addDecl(Fn);

  // If the function has a body, inject that also. Note that namespace-scope
  // function definitions are never deferred. Also, function decls never
  // appear in class scope (we hope), so we shouldn't be doing this too
  // early.
  if (Stmt *OldBody = D->getBody()) {
    Sema::SynthesizedFunctionScope Scope(getSema(), Fn);
    Sema::ContextRAII FnCtx (getSema(), Fn);
    StmtResult NewBody = TransformStmt(OldBody);
    if (NewBody.isInvalid())
      Fn->setInvalidDecl();
    else
      Fn->setBody(NewBody.get());
  }

  return Fn;
}

Decl *InjectionContext::InjectVarDecl(VarDecl *D) {
  DeclContext *Owner = getSema().CurContext;

  DeclarationNameInfo DNI;
  TypeSourceInfo *TSI;
  bool Invalid = InjectDeclarator(D, DNI, TSI);

  // FIXME: Check for re-declaration.

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
  Owner->addDecl(Var);

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

  if (GetModifiers().addConstexpr()) {
    Var->setConstexpr(true);
    Var->setType(Var->getType().withConst());
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

  // FIXME: Instantiate attributes.

  // Forward the mangling number from the template to the instantiated decl.
  getContext().setManglingNumber(
      Var, getContext().getManglingNumber(D));
  getContext().setStaticLocalNumber(
      Var, getContext().getStaticLocalNumber(D));

  if (D->isInlineSpecified())
    Var->setInlineSpecified();
  else if (D->isInline())
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

static bool ShouldImmediatelyInjectPendingDefinitions(
              Decl *Injectee, DeclContext *ClassOwner, bool InjectedIntoOwner) {
  // If we're injecting into a class, always defer.
  if (isa<CXXRecordDecl>(Injectee) && InjectedIntoOwner)
    return false;

  // Handle pending members, we've reached the injectee.
  if (Decl::castFromDeclContext(ClassOwner) == Injectee)
    return true;

  // Handle pending members, we've reached the root of the mock injection.
  if (!InjectedIntoOwner)
    return true;

  return false;
}

static void InjectPendingDefinitionsWithCleanup(InjectionContext *Ctx) {
  InjectionInfo *Injection = Ctx->CurInjection;

  InjectPendingDefinitions<FieldDecl, InjectedDef_Field>(Ctx, Injection);
  InjectPendingDefinitions<CXXMethodDecl, InjectedDef_Method>(Ctx, Injection);

  Injection->ResetClassMemberData();
}

Decl *InjectionContext::InjectCXXRecordDecl(CXXRecordDecl *D) {
  bool Invalid = false;
  DeclContext *Owner = getSema().CurContext;

  // FIXME: Do a lookup for previous declarations.

  CXXRecordDecl *Class;
  if (D->isInjectedClassName()) {
    DeclarationName DN = cast<CXXRecordDecl>(Owner)->getDeclName();
    Class = CXXRecordDecl::Create(
        getContext(), D->getTagKind(), Owner, D->getBeginLoc(),
        D->getLocation(), DN.getAsIdentifierInfo(), /*PrevDecl=*/nullptr);
  } else {
    DeclarationNameInfo DNI = TransformDeclarationName(D);
    if (!DNI.getName())
      Invalid = true;
    Class = CXXRecordDecl::Create(
        getContext(), D->getTagKind(), Owner, D->getBeginLoc(),
        D->getLocation(), DNI.getName().getAsIdentifierInfo(),
        /*PrevDecl=*/nullptr);
  }
  AddDeclSubstitution(D, Class);

  // FIXME: Inject attributes.

  // FIXME: Propagate other properties?
  Class->setAccess(D->getAccess());
  Class->setImplicit(D->isImplicit());
  Class->setInvalidDecl(Invalid);

  // Don't register the declaration if we're merely attempting to transform
  // this class.
  bool InjectIntoOwner = ShouldInjectInto(Owner);
  if (InjectIntoOwner)
    Owner->addDecl(Class);

  if (D->hasDefinition())
    InjectClassDefinition(*this, D, Class);

  if (ShouldImmediatelyInjectPendingDefinitions(
          Injectee, Owner, InjectIntoOwner))
    InjectPendingDefinitionsWithCleanup(this);

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

Decl *InjectionContext::InjectFieldDecl(FieldDecl *D) {
  if (GetModifiers().getStorageModifier() == StorageModifier::Static) {
    return InjectStaticDataMemberDecl(D);
  }

  DeclarationNameInfo DNI;
  TypeSourceInfo *TSI;
  CXXRecordDecl *Owner;
  bool Invalid = InjectMemberDeclarator(D, DNI, TSI, Owner);

  // FIXME: Substitute through the bit width.
  Expr *BitWidth = nullptr;

  // Build and check the field.
  FieldDecl *Field = getSema().CheckFieldDecl(
      DNI.getName(), TSI->getType(), TSI, Owner, D->getLocation(),
      D->isMutable(), BitWidth, D->getInClassInitStyle(), D->getInnerLocStart(),
      D->getAccess(), nullptr);
  AddDeclSubstitution(D, Field);

  // FIXME: Propagate attributes?

  // FIXME: In general, see VisitFieldDecl in the template instantiatior.
  // There are some interesting cases we probably need to handle.

  // Can't make
  if (GetModifiers().addConstexpr()) {
    SemaRef.Diag(D->getLocation(), diag::err_modify_constexpr_field);
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

static bool
ImplicitlyInstantiatedByFragment(TemplateSpecializationKind TemplateSK,
                                 FunctionDecl *TemplateFD) {
  if (TemplateSK != TSK_ImplicitInstantiation)
    return false;
  if (!TemplateFD->isInFragment())
    return false;

  DeclContext *DC = TemplateFD->getDeclContext();
  CXXRecordDecl *D = dyn_cast_or_null<CXXRecordDecl>(DC);
  return D && !D->isLocalClass();
}

Decl *InjectionContext::InjectCXXMethodDecl(CXXMethodDecl *D) {
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
                                        Ctor->isExplicit(),
                                        Ctor->isInlineSpecified(),
                                        Ctor->isImplicit(),
                                        Ctor->isConstexpr());
    Method->setRangeEnd(D->getEndLoc());
  } else if (CXXDestructorDecl *Dtor = dyn_cast<CXXDestructorDecl>(D)) {
    Method = CXXDestructorDecl::Create(AST, Owner, D->getBeginLoc(), DNI,
                                       TSI->getType(), TSI,
                                       Dtor->isInlineSpecified(),
                                       Dtor->isImplicit());
    Method->setRangeEnd(D->getEndLoc());
  } else if (CXXConversionDecl *Conv = dyn_cast<CXXConversionDecl>(D)) {
    Method = CXXConversionDecl::Create(AST, Owner, D->getBeginLoc(), DNI,
                                       TSI->getType(), TSI,
                                       Conv->isInlineSpecified(),
                                       Conv->isExplicit(), Conv->isConstexpr(),
                                       Conv->getEndLoc());
  } else {
    Method = CXXMethodDecl::Create(AST, Owner, D->getBeginLoc(), DNI,
                                   TSI->getType(), TSI,
                                   D->isStatic() ? SC_Static : SC_None,
                                   D->isInlineSpecified(), D->isConstexpr(),
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
    if (!ImplicitlyInstantiatedByFragment(TemplateSK, TemplateFD)) {
      Method->setInstantiationOfMemberFunction(TemplateFD, TemplateSK);
    }
  }

  // Propagate semantic properties.
  Method->setImplicit(D->isImplicit());
  ApplyAccess(GetModifiers(), Method, D);

  // Update the constexpr specifier.
  if (GetModifiers().addConstexpr()) {
    if (isa<CXXDestructorDecl>(Method)) {
      SemaRef.Diag(D->getLocation(), diag::err_constexpr_dtor);
      Method->setInvalidDecl(true);
    }
    Method->setConstexpr(true);
    Method->setType(Method->getType().withConst());
  } else {
    Method->setConstexpr(D->isConstexpr());
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
    // FIXME: Is this right?
    LookupResult Previous(
      SemaRef, Method->getDeclName(), SourceLocation(),
      Sema::LookupOrdinaryName, SemaRef.forRedeclarationInCurContext());
    SemaRef.LookupQualifiedName(Previous, Owner);

    getSema().CheckFunctionDeclaration(/*Scope=*/nullptr, Method, Previous,
                                       /*IsMemberSpecialization=*/false);
    Owner->addDecl(Method);
  }

  // If the method is has a body, add it to the context so that we can
  // process it later. Note that deleted/defaulted definitions are just
  // flags processed above. Ignore the definition if we've marked this
  // as pure virtual.
  if (D->hasBody() && !Method->isPure())
    AddPendingDefinition(InjectedDef(InjectedDef_Method, D, Method));

  return Method;
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
  case Decl::StaticAssert:
    return InjectStaticAssertDecl(cast<StaticAssertDecl>(D));
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
  if (!R)
    return nullptr;

  // If we injected a top-level declaration, notify the AST consumer,
  // so that it can be processed for code generation.
  if (isa<TranslationUnitDecl>(R->getDeclContext()))
    getSema().Consumer.HandleTopLevelDecl(DeclGroupRef(R));

  return R;
}

Decl *InjectionContext::MockInjectDecl(Decl *D) {
  DeclContext *OriginalMockInjectionContext = MockInjectionContext;
  MockInjectionContext = getSema().CurContext;

  // Run normal injection logic.
  Decl *NewDecl = InjectDecl(D);

  MockInjectionContext = OriginalMockInjectionContext;
  return NewDecl;
}

Decl *InjectionContext::InjectAccessSpecDecl(AccessSpecDecl *D) {
  CXXRecordDecl *Owner = cast<CXXRecordDecl>(getSema().CurContext);
  return AccessSpecDecl::Create(
      getContext(), D->getAccess(), Owner, D->getLocation(), D->getColonLoc());
}

template <typename MetaType>
static Decl *
InjectCXXMetaDecl(InjectionContext &Ctx, MetaType *D) {
  Sema &Sema = Ctx.getSema();

  // We can use the ActOn* members since the initial parsing for these
  // declarations is trivial (i.e., don't have to translate declarators).
  Decl *New = Sema.ActOnCXXMetaprogramDecl(D->getLocation());

  DeclContext *OriginalDC;
  Sema.ActOnStartCXXMetaprogramDecl(New, OriginalDC);

  StmtResult S = Ctx.TransformStmt(D->getBody());
  if (!S.isInvalid())
    Sema.ActOnFinishCXXMetaprogramDecl(New, S.get(), OriginalDC);
  else
    Sema.ActOnCXXMetaprogramDeclError(New, OriginalDC);

  return New;
}

Decl *InjectionContext::InjectCXXMetaprogramDecl(CXXMetaprogramDecl *D) {
  return InjectCXXMetaDecl(*this, D);
}

Decl *InjectionContext::InjectCXXInjectionDecl(CXXInjectionDecl *D) {
  return InjectCXXMetaDecl(*this, D);
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
  Decl *Pattern = MockInjectDecl(D->getTemplatedDecl());
  if (!Pattern)
    return nullptr;

  CXXRecordDecl *Class = cast<CXXRecordDecl>(Pattern);

  // Build the enclosing template.
  ClassTemplateDecl *Template = ClassTemplateDecl::Create(
       getSema().Context, getSema().CurContext, Class->getLocation(),
       Class->getDeclName(), Parms, Class);
  AddDeclSubstitution(D, Template);

  // FIXME: Other attributes to process?
  Class->setDescribedClassTemplate(Template);
  ApplyAccess(GetModifiers(), Template, D);

  // Don't register the declaration if we're merely attempting to transform
  // this template.
  if (ShouldInjectInto(Owner))
    Owner->addDecl(Template);

  return Template;
}

Decl *InjectionContext::InjectClassTemplateSpecializationDecl(
                                           ClassTemplateSpecializationDecl *D) {
  DeclContext *Owner = getSema().CurContext;

  Decl *Template = MockInjectDecl(D->getSpecializedTemplate());
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

Decl *InjectionContext::InjectFunctionTemplateDecl(FunctionTemplateDecl *D) {
  DeclContext *Owner = getSema().CurContext;

  TemplateParameterList *Parms = InjectTemplateParms(D->getTemplateParameters());
  if (!Parms)
    return nullptr;

  // Build the underlying pattern.
  Decl *Pattern = MockInjectDecl(D->getTemplatedDecl());
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

Decl* InjectionContext::InjectTemplateTypeParmDecl(TemplateTypeParmDecl *D) {
  TemplateTypeParmDecl *Parm = TemplateTypeParmDecl::Create(
      getSema().Context, getSema().CurContext, D->getBeginLoc(), D->getLocation(),
      D->getDepth(), D->getIndex(), D->getIdentifier(),
      D->wasDeclaredWithTypename(), D->isParameterPack());
  AddDeclSubstitution(D, Parm);

  Parm->setAccess(AS_public);

  // Process the default argument.
  if (D->hasDefaultArgument() && !D->defaultArgumentWasInherited()) {
    TypeSourceInfo *Default = TransformType(D->getDefaultArgumentInfo());
    if (Default)
      Parm->setDefaultArgument(Default);
    // FIXME: What if this fails.
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

} // namespace clang

static Decl *GetRootDeclaration(Expr *E) {
  if (auto *ICE = dyn_cast<ImplicitCastExpr>(E))
    return GetRootDeclaration(ICE->getSubExpr());

  if (auto *ASE = dyn_cast<ArraySubscriptExpr>(E))
    return GetRootDeclaration(ASE->getBase());

  if (auto *DRE = dyn_cast<DeclRefExpr>(E))
    return DRE->getDecl();

  if (auto *ME = dyn_cast<MemberExpr>(E))
    return ME->getMemberDecl();

  llvm_unreachable("failed to find declaration");
}

template<typename T, typename F>
static void FilteredCaptureCB(T *Entity, F CaptureCB) {
  QualType EntityTy = Entity->getType();

  if (EntityTy->isPointerType())
    return;

  if (EntityTy->canDecayToPointerType())
    return;

  if (EntityTy->isReferenceType())
    return;

  CaptureCB(Entity);
}

template<typename F>
static void ExtractDecomposedDeclInits(Sema &S, DecompositionDecl *D,
                                       F CapturedExprCB) {
  for (BindingDecl *BD : D->bindings()) {
    ExprResult LValueConverted = S.DefaultFunctionArrayLvalueConversion(BD->getBinding());
    FilteredCaptureCB(LValueConverted.get(), CapturedExprCB);
  }
}

/// Calls the passed lambda on any init exprs conatined in Expr E.
template<typename F>
static void ExtractCapturedVariableInits(Sema &S, Expr *E, F CapturedExprCB) {
  Decl *D = GetRootDeclaration(E);

  if (auto *DD = dyn_cast<DecompositionDecl>(D)) {
    ExtractDecomposedDeclInits(S, DD, CapturedExprCB);
    return;
  }

  FilteredCaptureCB(E, CapturedExprCB);
}

template<typename F>
static void ExtractDecomposedDecls(Sema &S, DecompositionDecl *D,
                                   F CapturedDeclCB) {
  for (BindingDecl *BD : D->bindings()) {
    FilteredCaptureCB(BD, CapturedDeclCB);
  }
}

/// Calls the passed lambda on any variable declarations
/// conatined in Expr E.
template<typename F>
static void ExtractCapturedVariables(Sema &S, Expr *E, F CapturedDeclCB) {
  Decl *D = GetRootDeclaration(E);

  if (auto *DD = dyn_cast<DecompositionDecl>(D)) {
    ExtractDecomposedDecls(S, DD, CapturedDeclCB);
    return;
  }

  FilteredCaptureCB(cast<ValueDecl>(D), CapturedDeclCB);
}

// Find variables to capture in the given scope.
static void FindCapturesInScope(Sema &SemaRef, Scope *S,
                                SmallVectorImpl<ValueDecl *> &Vars) {
  for (Decl *D : S->decls()) {
    if (VarDecl *Var = dyn_cast<VarDecl>(D)) {
      // Only capture locals with initializers.
      //
      // FIXME: If the fragment is in the initializer of a variable, this
      // will also capture that variable. For example:
      //
      //    auto f = <<class: ... >>;
      //
      // The capture list for the fragment will include f. This seems insane,
      // but lambda capture seems to also do this (with some caveats about
      // usage).
      //
      // We can actually detect this case in this implementation because
      // the type must be deduced and we won't have associated the
      // initializer with the variable yet.
      if (!isa<ParmVarDecl>(Var) &&
          !Var->hasInit() &&
          Var->getType()->isUndeducedType())
        continue;

      if (auto *DD = dyn_cast<DecompositionDecl>(D)) {
        ExtractDecomposedDecls(SemaRef, DD, [&] (ValueDecl *DV) { Vars.push_back(DV); });
        return;
      }

      FilteredCaptureCB(cast<ValueDecl>(D), [&] (ValueDecl *DV) { Vars.push_back(DV); });
    }
  }
}

// Search the scope list for captured variables. When S is null, we're
// applying applying a transformation.
static void FindCaptures(Sema &SemaRef, Scope *S, FunctionDecl *Fn,
                         SmallVectorImpl<ValueDecl *> &Vars) {
  assert(S && "Expected non-null scope");
  while (S && S->getEntity() != Fn) {
    FindCapturesInScope(SemaRef, S, Vars);
    S = S->getParent();
  }
  if (S)
    FindCapturesInScope(SemaRef, S, Vars);
}

/// Construct a reference to each captured value and force an r-value
/// conversion so that we get rvalues during evaluation.
static void ReferenceCaptures(Sema &SemaRef,
                              SmallVectorImpl<ValueDecl *> &Vars,
                              SmallVectorImpl<Expr *> &Refs) {
  Refs.resize(Vars.size());
  std::transform(Vars.begin(), Vars.end(), Refs.begin(), [&](ValueDecl *D) {
    Expr *Ref = new (SemaRef.Context) DeclRefExpr(
        SemaRef.Context, D, false, D->getType(), VK_LValue, D->getLocation());
    return ImplicitCastExpr::Create(SemaRef.Context, D->getType(),
                                    CK_LValueToRValue, Ref, nullptr, VK_RValue);
  });
}

// Create a placeholder for each captured expression in the scope of the
// fragment. For some captured variable 'v', these have the form:
//
//    constexpr auto v = <opaque>;
//
// These are replaced by their values during injection.
static void CreatePlaceholder(Sema &SemaRef, CXXFragmentDecl *Frag, Expr *E) {
  ExtractCapturedVariables(SemaRef, E, [&] (ValueDecl *Var) {
    SourceLocation NameLoc = Var->getLocation();
    DeclarationName Name = Var->getDeclName();
    QualType T = SemaRef.Context.DependentTy;
    TypeSourceInfo *TSI = SemaRef.Context.getTrivialTypeSourceInfo(T);
    VarDecl *Placeholder = VarDecl::Create(SemaRef.Context, Frag, NameLoc, NameLoc,
                                           Name, T, TSI, SC_Static);
    Placeholder->setConstexpr(true);
    Placeholder->setImplicit(true);
    Placeholder->setInitStyle(VarDecl::CInit);
    Placeholder->setInit(
        new (SemaRef.Context) OpaqueValueExpr(NameLoc, T, VK_RValue));
    Placeholder->setReferenced(true);
    Placeholder->markUsed(SemaRef.Context);
    Frag->addDecl(Placeholder);
  });
}

static void CreatePlaceholders(Sema &SemaRef, CXXFragmentDecl *Frag,
                               SmallVectorImpl<Expr *> &Captures) {
  std::for_each(Captures.begin(), Captures.end(), [&](Expr *E) {
    CreatePlaceholder(SemaRef, Frag, E);
  });
}

/// Called at the start of a source code fragment to establish the list of
/// automatic variables captured. This is only called by the parser and searches
/// the list of local variables in scope.
void Sema::ActOnCXXFragmentCapture(SmallVectorImpl<Expr *> &Captures) {
  assert(Captures.empty() && "Captures already specified");

  // Only collect captures within a function.
  //
  // FIXME: It might be better to use the scope, but the flags don't appear
  // to be set right within constexpr declarations, etc.
  if (isa<FunctionDecl>(CurContext)) {
    SmallVector<ValueDecl *, 8> Vars;
    FindCaptures(*this, CurScope, getCurFunctionDecl(), Vars);
    ReferenceCaptures(*this, Vars, Captures);
  }
}

/// Called at the start of a source code fragment to establish the fragment
/// declaration and placeholders.
Decl *Sema::ActOnStartCXXFragment(Scope* S, SourceLocation Loc,
                                  SmallVectorImpl<Expr *> &Captures) {
  CXXFragmentDecl *Fragment = CXXFragmentDecl::Create(Context, CurContext, Loc);
  CreatePlaceholders(*this, Fragment, Captures);

  if (S)
    PushDeclContext(S, Fragment);

  return Fragment;
}


/// Binds the content the fragment declaration. Returns the updated fragment.
/// The Fragment is nullptr if an error occurred during parsing. However,
/// we still need to pop the declaration context.
Decl *Sema::ActOnFinishCXXFragment(Scope *S, Decl *Fragment, Decl *Content) {
  CXXFragmentDecl *FD = nullptr;
  if (Fragment) {
    FD = cast<CXXFragmentDecl>(Fragment);
    FD->setContent(Content);
  }

  if (S)
    PopDeclContext();

  return FD;
}


/// Builds a new fragment expression.
ExprResult Sema::ActOnCXXFragmentExpr(SourceLocation Loc, Decl *Fragment,
                                      SmallVectorImpl<Expr *> &Captures) {
  return BuildCXXFragmentExpr(Loc, Fragment, Captures);
}

/// Builds a new CXXFragmentExpr using the provided reflection, and
/// captures.
///
/// Fragment expressions create a new class, defined approximately,
/// like this:
///
///   struct __fragment_type  {
///     // Reflection of decl to be injected
///     meta::info fragment_reflection;
///
///     // Fragment captures
///     auto __param_<cap_1_identifier_name>;
///     ...
///     auto __param_<cap_n_identifier_name>;
///   };
///
static
CXXFragmentExpr *SynthesizeFragmentExpr(Sema &S,
                                        SourceLocation Loc,
                                        CXXFragmentDecl *FD,
                                        CXXReflectExpr *Reflection,
                                        SmallVectorImpl<Expr *> &Captures) {
  ASTContext &Context = S.Context;
  DeclContext *CurContext = S.CurContext;

  // Build our new class implicit class to hold our fragment info.
  CXXRecordDecl *Class = CXXRecordDecl::Create(Context, TTK_Class, CurContext,
                                               Loc, Loc,
                                               /*Id=*/nullptr,
                                               /*PrevDecl=*/nullptr);
  S.StartDefinition(Class);

  Class->setImplicit(true);
  Class->setFragment(true);

  QualType ClassTy = Context.getRecordType(Class);
  TypeSourceInfo *ClassTSI = Context.getTrivialTypeSourceInfo(ClassTy);


  // Build the class fields.
  SmallVector<FieldDecl *, 4> Fields;

  // Build the field for the reflection itself.
  QualType ReflectionType = Reflection->getType();
  IdentifierInfo *ReflectionFieldId = &Context.Idents.get(
      "fragment_reflection");
  TypeSourceInfo *ReflectionTypeInfo = Context.getTrivialTypeSourceInfo(
      ReflectionType);

  QualType ConstReflectionType = ReflectionType.withConst();
  FieldDecl *Field = FieldDecl::Create(Context, Class, Loc, Loc,
                                       ReflectionFieldId, ConstReflectionType,
                                       ReflectionTypeInfo,
                                       nullptr, false,
                                       ICIS_NoInit);
  Field->setAccess(AS_public);
  Field->setImplicit(true);

  Fields.push_back(Field);
  Class->addDecl(Field);

  // Build the capture fields.
  for (Expr *E : Captures) {
    ExtractCapturedVariables(S, E, [&] (ValueDecl *Var)  {
      std::string Name = "__captured_" + Var->getIdentifier()->getName().str();
      IdentifierInfo *Id = &Context.Idents.get(Name);

      // We need to get the non-reference type, we're capturing by value.
      QualType Type = Var->getType().getNonReferenceType();

      TypeSourceInfo *TypeInfo = Context.getTrivialTypeSourceInfo(Type);
      FieldDecl *Field = FieldDecl::Create(Context, Class, Loc, Loc, Id,
                                           Type, TypeInfo,
                                           nullptr, false,
                                           ICIS_NoInit);
      Field->setAccess(AS_public);
      Field->setImplicit(true);

      Fields.push_back(Field);
      Class->addDecl(Field);
    });
  }

  // Build a new constructor for our fragment type.
  DeclarationName Name = Context.DeclarationNames.getCXXConstructorName(
      Context.getCanonicalType(ClassTy));
  DeclarationNameInfo NameInfo(Name, Loc);
  CXXConstructorDecl *Ctor = CXXConstructorDecl::Create(
      Context, Class, Loc, NameInfo, /*Type*/QualType(), /*TInfo=*/nullptr,
      /*isExplicit=*/true, /*isInline=*/true, /*isImplicitlyDeclared=*/false,
      /*isConstexpr=*/true);
  Ctor->setAccess(AS_public);

  // Build the function type for said constructor.
  FunctionProtoType::ExtProtoInfo EPI;
  EPI.ExceptionSpec.Type = EST_Unevaluated;
  EPI.ExceptionSpec.SourceDecl = Ctor;
  EPI.ExtInfo = EPI.ExtInfo.withCallingConv(
      Context.getDefaultCallingConvention(/*IsVariadic=*/false,
                                          /*IsCXXMethod=*/true));

  SmallVector<QualType, 4> ArgTypes;
  ArgTypes.push_back(ReflectionType);
  for (Expr *E : Captures) {
    ExtractCapturedVariableInits(S, E, [&] (Expr *EE) {
      ArgTypes.push_back(EE->getType());
    });
  }

  QualType CtorTy = Context.getFunctionType(Context.VoidTy, ArgTypes, EPI);
  Ctor->setType(CtorTy);

  // Build the constructor params.
  SmallVector<ParmVarDecl *, 4> Parms;

  // Build the constructor param for the reflection param.
  IdentifierInfo *ReflectionParmId = &Context.Idents.get("fragment_reflection");
  ParmVarDecl *Parm = ParmVarDecl::Create(Context, Ctor, Loc, Loc,
                                          ReflectionParmId,
                                          ReflectionType, ReflectionTypeInfo,
                                          SC_None, nullptr);
  Parm->setScopeInfo(0, 0);
  Parm->setImplicit(true);
  Parms.push_back(Parm);

  // Build the constructor capture params.
  int DestExpansion = 0;
  for (Expr *E : Captures) {
    ExtractCapturedVariables(S, E, [&] (ValueDecl *Var) {
      std::string Name = "__param_" + Var->getIdentifier()->getName().str();
      IdentifierInfo *Id = &Context.Idents.get(Name);

      // We need to get the non-reference type, we're capturing by value.
      QualType ParmTy = Var->getType().getNonReferenceType();

      TypeSourceInfo *TypeInfo = Context.getTrivialTypeSourceInfo(ParmTy);
      ParmVarDecl *Parm = ParmVarDecl::Create(Context, Ctor, Loc, Loc,
                                              Id, ParmTy, TypeInfo,
                                              SC_None, nullptr);
      Parm->setScopeInfo(0, ++DestExpansion);
      Parm->setImplicit(true);
      Parms.push_back(Parm);
    });
  }

  Ctor->setParams(Parms);

  // Build constructor initializers.
  std::size_t NumInits = Fields.size();
  CXXCtorInitializer **Inits = new (Context) CXXCtorInitializer *[NumInits];

  // Build member initializers.
  for (std::size_t I = 0; I < Parms.size(); ++I) {
    ParmVarDecl *Parm = Parms[I];
    FieldDecl *Field = Fields[I];
    QualType Type = Parm->getType();
    DeclRefExpr *Ref = new (Context) DeclRefExpr(
        Context, Parm, false, Type, VK_LValue, Loc);
    Expr *Arg = ParenListExpr::Create(Context, Loc, Ref, Loc);
    Inits[I] = S.BuildMemberInitializer(Field, Arg, Loc).get();
  }
  Ctor->setNumCtorInitializers(NumInits);
  Ctor->setCtorInitializers(Inits);

  // Build the definition.
  Stmt *Def = CompoundStmt::Create(Context, None, Loc, Loc);
  Ctor->setBody(Def);
  Class->addDecl(Ctor);

  S.CompleteDefinition(Class);

  // Setup the arguments to use for initialization.
  SmallVector<Expr *, 8> CtorArgs;
  CtorArgs.push_back(dyn_cast<Expr>(Reflection));
  for (Expr *E : Captures) {
    ExtractCapturedVariableInits(S, E, [&] (Expr *EE) {
      CtorArgs.push_back(EE);
    });
  }

  // Build an expression that that initializes the fragment object.
  Expr *Init;
  if (CtorArgs.size() == 1) {
    CXXConstructExpr *Cast = CXXConstructExpr::Create(
        Context, ClassTy, Loc, Ctor, true, CtorArgs,
        /*HadMultipleCandidates=*/false, /*ListInitialization=*/false,
        /*StdInitListInitialization=*/false, /*ZeroInitialization=*/false,
        CXXConstructExpr::CK_Complete, SourceRange(Loc, Loc));
    Init = CXXFunctionalCastExpr::Create(
        Context, ClassTy, VK_RValue, ClassTSI, CK_NoOp, Cast,
        /*Path=*/nullptr, Loc, Loc);
  } else {
    Init = CXXTemporaryObjectExpr::Create(
        Context, Ctor, ClassTy, ClassTSI, CtorArgs, SourceRange(Loc, Loc),
        /*HadMultipleCandidates=*/false, /*ListInitialization=*/false,
        /*StdInitListInitialization=*/false, /*ZeroInitialization=*/false);
  }

  // Finally, build the fragment expression.
  return new (Context) CXXFragmentExpr(Context, Loc, ClassTy, FD, Captures,
                                       Init);
}

/// Builds a new fragment expression.
ExprResult Sema::BuildCXXFragmentExpr(SourceLocation Loc, Decl *Fragment,
                                      SmallVectorImpl<Expr *> &Captures) {
  CXXFragmentDecl *FD = cast<CXXFragmentDecl>(Fragment);

  // If the fragment appears in a context that depends on template parameters,
  // then the expression is dependent.
  //
  // FIXME: This is just an approximation of the right answer. In truth, the
  // expression is dependent if the fragment depends on any template parameter
  // in this or any enclosing context.
  if (CurContext->isDependentContext()) {
    return new (Context) CXXFragmentExpr(Context, Loc, Context.DependentTy,
                                         FD, Captures, nullptr);
  }

  // Build the expression used to the reflection of fragment.
  ExprResult Reflection = BuildCXXReflectExpr(/*Loc*/SourceLocation(),
                                              FD->getContent(),
                                              /*LP=*/SourceLocation(),
                                              /*RP=*/SourceLocation());
  if (Reflection.isInvalid())
    return ExprError();

  return SynthesizeFragmentExpr(
      *this, Loc, FD, static_cast<CXXReflectExpr *>(Reflection.get()),
      Captures);
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
  // If the operand is not dependent, it must be resolveable either
  // to an injectable reflection, or a fragment.
  //
  // An injectable reflection is defined as any reflection which
  // we can resolve to a declaration.
  bool IsDependent = isTypeOrValueDependent(Operand);
  if (!IsDependent && !CheckInjectionOperand(*this, Operand)) {
    return StmtError();
  }

  // Perform an lvalue-to-value conversion so that we get an rvalue in
  // evaluation.
  if (Operand->isGLValue())
    Operand = ImplicitCastExpr::Create(Context, Operand->getType(),
                                       CK_LValueToRValue, Operand,
                                       nullptr, VK_RValue);

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

// Returns an integer value describing the target context of the injection.
// This correlates to the second %select in err_invalid_injection.
static int DescribeDeclContext(DeclContext *DC) {
  if (DC->isFunctionOrMethod())
    return 0;
  else if (DC->isRecord())
    return 1;
  else if (DC->isNamespace())
    return 2;
  else if (DC->isTranslationUnit())
    return 3;
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

  auto ClassTest = [] (DeclContext *DC) -> bool {
    return DC->isRecord();
  };
  if (Check(ClassTest))
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

static InjectionContext *FindInjectionContext(Sema &S, Decl *Injectee) {
  if (InjectionContext *LastCtx = GetLastInjectionContext(S)) {
    if (LastCtx->Injectee == Injectee) {
      return LastCtx;
    }
  }

  return new InjectionContext(S, Injectee);
}

template<typename IT, typename F>
static bool BootstrapInjection(Sema &S, Decl *Injectee, IT *Injection,
                               F InjectionProcedure) {
  // Create an injection context and then execute the logic for that
  // context.
  InjectionContext *Ctx = FindInjectionContext(S, Injectee);
  Ctx->InitInjection(Injection, [&InjectionProcedure, &Ctx] {
    InjectionProcedure(Ctx);
  });

  bool NewContext = GetLastInjectionContext(S) != Ctx;
  if (NewContext) {
    if (Ctx->hasPendingInjections()) {
      S.PendingClassMemberInjections.push_back(Ctx);
      return true;
    }

    delete Ctx;
  }

  return !Injectee->isInvalidDecl();
}

/// Inject a fragment into the current context.
static bool InjectFragment(Sema &S,
                           SourceLocation POI,
                           Decl *Injection,
                           const SmallVector<InjectionCapture, 8> &Captures,
                           Decl *Injectee) {
  DeclContext *InjectionAsDC = Decl::castToDeclContext(Injection);
  DeclContext *InjecteeAsDC = Decl::castToDeclContext(Injectee);

  if (!CheckInjectionContexts(S, POI, InjectionAsDC, InjecteeAsDC))
    return false;

  Sema::ContextRAII Switch(S, InjecteeAsDC, isa<CXXRecordDecl>(Injectee));

  return BootstrapInjection(S, Injectee, Injection, [&](InjectionContext *Ctx) {
    // Setup substitutions
    Ctx->MaybeAddDeclSubstitution(Injection, Injectee);
    Ctx->AddPlaceholderSubstitutions(Injection->getDeclContext(), Captures);

    // Inject each declaration in the fragment.
    for (Decl *D : InjectionAsDC->decls()) {
      // Never inject injected class names.
      if (CXXRecordDecl *Class = dyn_cast<CXXRecordDecl>(D))
        if (Class->isInjectedClassName())
          continue;

      Decl *R = Ctx->InjectDecl(D);
      if (!R || R->isInvalidDecl()) {
        Injectee->setInvalidDecl(true);
        continue;
      }
    }
  });
}

// Inject a reflected base specifier into the current context.
static bool AddBase(Sema &S, SourceLocation POI,
                    CXXBaseSpecifier *Injection, Decl *Injectee) {
  // DeclContext *InjectionDC = Injection->getDeclContext();
  // Decl *InjectionOwner = Decl::castFromDeclContext(InjectionDC);
  DeclContext *InjecteeAsDC = Decl::castToDeclContext(Injectee);

  // FIXME: Ensure we're injecting into a class.
  // if (!CheckInjectionContexts(S, POI, InjectionDC, InjecteeAsDC))
  //   return false;

  // Establish injectee as the current context.
  Sema::ContextRAII Switch(S, InjecteeAsDC, isa<CXXRecordDecl>(Injectee));

  return BootstrapInjection(S, Injectee, Injection, [&](InjectionContext *Ctx) {
    // Inject the base specifier.
    if (Ctx->InjectBaseSpecifier(Injection))
      Injectee->setInvalidDecl(true);
  });
}

// Inject a reflected declaration into the current context.
static bool CopyDeclaration(Sema &S, SourceLocation POI,
                            Decl *Injection,
                            const ReflectionModifiers &Modifiers,
                            Decl *Injectee) {
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

    // Inject the declaration.
    Decl* Result = Ctx->InjectDecl(Injection);
    if (!Result || Result->isInvalidDecl()) {
      Injectee->setInvalidDecl(true);
    }
  });
}

static Reflection
GetReflectionFromFrag(Sema &S, InjectionEffect &IE) {
  const int REFLECTION_INDEX = 0;

  APValue FragmentData = IE.ExprValue;
  APValue APRefl = FragmentData.getStructField(REFLECTION_INDEX);

  return Reflection(S.Context, APRefl);
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

static const Decl *
GetFragInjectionDecl(Sema &S, InjectionEffect &IE) {
  Reflection &&Refl = GetReflectionFromFrag(S, IE);
  const Decl *Decl = Refl.getAsDeclaration();

  // Verify that our fragment contained a declaration.
  assert(Decl);
  return Decl;
}

static SmallVector<InjectionCapture, 8>
GetFragCaptures(InjectionEffect &IE) {
  // Setup field decls for iteration.
  CXXRecordDecl *FragmentClosureDecl = IE.ExprType->getAsCXXRecordDecl();
  assert(FragmentClosureDecl);
  auto DeclIterator    = FragmentClosureDecl->field_begin();
  auto DeclIteratorEnd = FragmentClosureDecl->field_end();

  // Setup field values for iteration.
  APValue FragmentData = IE.ExprValue;

  // Do not include the first field, it's used to store the
  // reflection not a capture.
  unsigned NumCaptures = FragmentData.getStructNumFields() - 1;
  DeclIterator++;

  // Allocate space in advanced as we know the size.
  SmallVector<InjectionCapture, 8> Captures;
  Captures.reserve(NumCaptures);

  // Map the capture decls to their values.
  for (unsigned int I = 0; I < NumCaptures; ++I) {
    assert(DeclIterator != DeclIteratorEnd);

    auto &&CaptureDecl = *DeclIterator++;
    auto &&CaptureValue = FragmentData.getStructField(I + 1);

    Captures.emplace_back(CaptureDecl, CaptureValue);
  }

  // Verify that all captures were copied,
  // and by association, that the number of
  // values matches the number of decls.
  assert(DeclIterator == DeclIteratorEnd);
  return Captures;
}

static bool ApplyFragmentInjection(Sema &S, SourceLocation POI,
                                   InjectionEffect &IE, Decl *Injectee) {
  Decl *Injection = const_cast<Decl *>(GetFragInjectionDecl(S, IE));
  SmallVector<InjectionCapture, 8> &&Captures = GetFragCaptures(IE);
  return InjectFragment(S, POI, Injection, Captures, Injectee);
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

static bool
ApplyReflectionBaseInjection(Sema &S, SourceLocation POI,
                             const Reflection &Refl, Decl *Injectee) {
  CXXBaseSpecifier *Injection = GetInjectionBase(Refl);

  return AddBase(S, POI, Injection, Injectee);
}

static Decl *
GetInjectionDecl(const Reflection &Refl) {
  const Decl *ReachableDecl = Refl.getAsReachableDeclaration();

  // Verify that our reflection contains a reachable declaration.
  assert(ReachableDecl);
  return const_cast<Decl *>(ReachableDecl);
}

static const ReflectionModifiers &
GetModifiers(const Reflection &Refl) {
  return Refl.getModifiers();
}

static bool
ApplyReflectionDeclInjection(Sema &S, SourceLocation POI,
                             const Reflection &Refl, Decl *Injectee) {
  Decl *Injection = GetInjectionDecl(Refl);
  ReflectionModifiers Modifiers = GetModifiers(Refl);

  return CopyDeclaration(S, POI, Injection, Modifiers, Injectee);
}

static bool ApplyReflectionInjection(Sema &S, SourceLocation POI,
                                     InjectionEffect &IE, Decl *Injectee) {
  Reflection &&Refl = GetReflectionFromInjection(S, IE);

  if (Refl.isBase())
    return ApplyReflectionBaseInjection(S, POI, Refl, Injectee);

  return ApplyReflectionDeclInjection(S, POI, Refl, Injectee);
}

bool Sema::ApplyInjection(SourceLocation POI, InjectionEffect &IE) {
  Decl *Injectee = GetInjecteeDecl(*this, CurContext, IE.ContextSpecifier);
  if (!Injectee)
    return false;

  // FIXME: We need to validate the Injection is compatible
  // with the Injectee.

  if (IE.ExprType->isFragmentType()) {
    return ApplyFragmentInjection(*this, POI, IE, Injectee);
  }

  // Type checking should gauarantee that the type of
  // our injection is either a Fragment or reflection.
  // Since we failed the fragment check, we must have
  // a reflection.
  assert(IE.ExprType->isReflectionType());

  return ApplyReflectionInjection(*this, POI, IE, Injectee);
}

/// Inject a sequence of source code fragments or modification requests
/// into the current AST. The point of injection (POI) is the point at
/// which the injection is applied.
///
/// returns true if no errors are encountered, false otherwise.
bool Sema::ApplyEffects(SourceLocation POI,
                        SmallVectorImpl<InjectionEffect> &Effects) {
  bool Ok = true;
  for (InjectionEffect &Effect : Effects) {
    Ok &= ApplyInjection(POI, Effect);
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

static void CleanupUsedContexts(
      llvm::SmallVectorImpl<InjectionContext *>& PendingClassMemberInjections) {
  while (!PendingClassMemberInjections.empty()) {
    delete PendingClassMemberInjections.pop_back_val();
  }
}

void Sema::InjectPendingFieldDefinitions() {
  for (auto &&Ctx : PendingClassMemberInjections) {
    InjectPendingFieldDefinitions(Ctx);
  }
}

void Sema::InjectPendingMethodDefinitions() {
  for (auto &&Ctx : PendingClassMemberInjections) {
    InjectPendingMethodDefinitions(Ctx);
  }
  CleanupUsedContexts(PendingClassMemberInjections);
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

  Sema::SynthesizedFunctionScope Scope(S, NewMethod);

  Sema::ContextRAII MethodCtx(S, NewMethod);
  StmtResult Body = Ctx->TransformStmt(OldMethod->getBody());
  if (Body.isInvalid())
    NewMethod->setInvalidDecl();
  else
    NewMethod->setBody(Body.get());

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

template<typename DeclType, InjectedDefType DefType>
static void InjectPendingDefinitions(InjectionContext *Ctx,
                                     InjectionInfo *Injection) {
  for (InjectedDef Def : Injection->InjectedDefinitions) {
    if (Def.Type != DefType)
      continue;

    InjectPendingDefinition(Ctx,
                            static_cast<DeclType *>(Def.Fragment),
                            static_cast<DeclType *>(Def.Injected));
  }
}

template<typename DeclType, InjectedDefType DefType>
static void InjectAllPendingDefinitions(InjectionContext *Ctx) {
  Ctx->ForEachPendingInjection([&Ctx] {
    InjectPendingDefinitions<DeclType, DefType>(Ctx, Ctx->CurInjection);
  });
}

void Sema::InjectPendingFieldDefinitions(InjectionContext *Ctx) {
  InjectAllPendingDefinitions<FieldDecl, InjectedDef_Field>(Ctx);
}

void Sema::InjectPendingMethodDefinitions(InjectionContext *Ctx) {
  InjectAllPendingDefinitions<CXXMethodDecl, InjectedDef_Method>(Ctx);
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
                           /*isConstexprSpecified=*/true);
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

  bool Folded = Call->EvaluateAsRValue(Result, Context);
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
      else if (Notes[0].second.getDiagID() != diag::note_constexpr_uninitialized) {
        // FIXME: These source locations are wrong.
        Sema.Diag(MD->getBeginLoc(), diag::err_expr_not_ice) << LangOpts.CPlusPlus;
        for (const PartialDiagnosticAt &Note : Notes)
          Sema.Diag(Note.first, Note.second);
      }
    }
  }

  // Apply any modifications, and if successful, remove the declaration from
  // the class; it shouldn't be visible in the output code.
  SourceLocation POI = MD->getSourceRange().getEnd();
  Sema.ApplyEffects(POI, Effects);

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
  DeclRefExpr *Ref =
      new (Context) DeclRefExpr(Context, D,
                                /*RefersToEnclosingVariableOrCapture=*/false,
                                FunctionTy, VK_LValue, SourceLocation());
  QualType PtrTy = Context.getPointerType(FunctionTy);
  ImplicitCastExpr *Cast =
      ImplicitCastExpr::Create(Context, PtrTy, CK_FunctionToPointerDecay, Ref,
                               /*BasePath=*/nullptr, VK_RValue);
  CallExpr *Call =
      CallExpr::Create(Context, Cast, ArrayRef<Expr *>(), Context.VoidTy,
                       VK_RValue, SourceLocation());
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
ActOnFinishMetaDecl(Sema &Sema, Decl *D, Stmt *Body, DeclContext *OriginalDC) {
  MetaType *MD = cast<MetaType>(D);

  FunctionDecl *Fn = MD->getFunctionDecl();

  Sema.DiscardCleanupsInEvaluationContext();
  Sema.ActOnFinishFunctionBody(Fn, Body);

  Sema.CurContext = OriginalDC;

  if (!Sema.CurContext->isDependentContext())
    EvaluateMetaDecl(Sema, MD, Fn);
}

/// Called immediately after parsing the body of a metaprorgam-declaration.
///
/// The statements within the body are evaluated here.
///
/// This function additionally ensures that the
/// declaration context is restored correctly.
void Sema::ActOnFinishCXXMetaprogramDecl(Decl *D, Stmt *Body,
                                         DeclContext *OriginalDC) {
  ActOnFinishMetaDecl<CXXMetaprogramDecl>(*this, D, Body, OriginalDC);
}

/// Called immediately after parsing the body of a injection-declaration.
///
/// The statements within the body are evaluated here.
///
/// This function additionally ensures that the
/// declaration context is restored correctly.
void Sema::ActOnFinishCXXInjectionDecl(Decl *D, Stmt *InjectionStmt,
                                       DeclContext *OriginalDC) {
  CompoundStmt *Body = CompoundStmt::Create(Context,
                                            ArrayRef<Stmt *>(InjectionStmt),
                                            SourceLocation(), SourceLocation());
  ActOnFinishMetaDecl<CXXInjectionDecl>(*this, D, Body, OriginalDC);
}

template <typename MetaType>
static void
ActOnCXXMetaError(Sema &Sema, Decl *D, DeclContext *OriginalDC) {
  MetaType *MD = cast<MetaType>(D);
  MD->setInvalidDecl();
  FunctionDecl *Fn = MD->getFunctionDecl();
  Sema.ActOnStartOfFunctionDef(nullptr, Fn);
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
    CheckCompletedCXXClass(Class);

    ActOnFinishCXXNonNestedClass(Class);

    assert(Class->isCompleteDefinition() && "Generated class not complete");
  }

  return Class;
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
