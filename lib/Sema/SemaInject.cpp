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
//===----------------------------------------------------------------------===g//

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

  using InjectionType = llvm::PointerUnion3<Decl *, CXXBaseSpecifier *, Stmt *>;
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

      CurInjection->PlaceholderSubsts.try_emplace(Var, Ty, Val);
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
    auto &TransformedDecls = GetDeclTransformMap();
    auto Iter = TransformedDecls.find(D);
    if (Iter != TransformedDecls.end())
      return Iter->second;
    else
      return nullptr;
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
      return new (getContext()) CXXConstantExpr(Opaque, TV.Value);
    } else {
      return nullptr;
    }
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

  QualType RebuildCXXRequiredTypeType(CXXRequiredTypeDecl *D) {
    QualType RequiredType = GetRequiredType(D);

    // Case 1, we've found a replacement type, use it.
    if (!RequiredType.isNull()) {
      return RequiredType;
    }

    // Case 2, we haven't found a replacement type, but
    // we're only trying to rebuild currently, perform default
    // transform.
    if (MockInjectionContext) {
      using Base = TreeTransform<InjectionContext>;
      return Base::RebuildCXXRequiredTypeType(D);
    }

    // Case 3, we haven't found a replacement type, and we
    // aren't rebuilding, an error should've been emitted during
    // injection of the RequirdeTypeDecl.
    return QualType();
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
  Decl *InjectFriendDecl(FriendDecl *D);
  Decl *InjectCXXMetaprogramDecl(CXXMetaprogramDecl *D);
  Decl *InjectCXXInjectionDecl(CXXInjectionDecl *D);

  TemplateParameterList *InjectTemplateParms(TemplateParameterList *Old);
  Decl *InjectClassTemplateDecl(ClassTemplateDecl *D);
  Decl *InjectClassTemplateSpecializationDecl(ClassTemplateSpecializationDecl *D);
  Decl *InjectFunctionTemplateDecl(FunctionTemplateDecl *D);
  Decl *InjectTemplateTypeParmDecl(TemplateTypeParmDecl *D);
  Decl *InjectNonTypeTemplateParmDecl(NonTypeTemplateParmDecl *D);
  Decl *InjectTemplateTemplateParmDecl(TemplateTemplateParmDecl *D);
  Decl *InjectStaticAssertDecl(StaticAssertDecl *D);
  Decl *InjectEnumDecl(EnumDecl *D);
  Decl *InjectEnumConstantDecl(EnumConstantDecl *D);
  Decl *InjectCXXStmtFragmentDecl(CXXStmtFragmentDecl *D);
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

  /// The DeclContext we're mock injecting for.
  DeclContext *MockInjectionContext = nullptr;

  /// The context into which the fragment is injected
  Decl *Injectee;

  /// The current injection being injected.
  InjectionInfo *CurInjection = nullptr;

  /// The pending class member injections.
  llvm::SmallVector<InjectionInfo *, 8> PendingInjections;

  /// If this is a local block fragment, we will inject it into
  /// a statement rather than a context.
  Stmt *InjecteeStmt;

  /// A container that holds the injected stmts we will eventually
  /// build a new CompoundStmt out of.
  llvm::SmallVector<Stmt *, 8> InjectedStmts;

  /// The declarations which have been injected.
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

  SourceLocation &&NamespaceLoc = D->getBeginLoc();
  SourceLocation &&Loc = D->getLocation();

  bool IsInline = D->isInline();
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

Decl *InjectionContext::InjectFunctionDecl(FunctionDecl *D) {
  DeclContext *Owner = getSema().CurContext;

  DeclarationNameInfo DNI;
  TypeSourceInfo* TSI;
  bool Invalid = InjectDeclarator(D, DNI, TSI);

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

  // Handle pending members, we've reached the root of the mock injection.
  if (!InjectedIntoOwner)
    return true;

  return false;
}

static void InjectPendingDefinitionsWithCleanup(InjectionContext &Ctx) {
  InjectionInfo *Injection = Ctx.CurInjection;

  InjectPendingDefinitions<FieldDecl, InjectedDef_Field>(&Ctx, Injection);
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

  if (D->isRequired())
    return Method;

  AddDeclSubstitution(D, Method);
  UpdateFunctionParms(D, Method);

  // Don't register the declaration if we're merely attempting to transform
  // this method.
  if (ShouldInjectInto(Owner)) {
    CheckInjectedFunctionDecl(getSema(), Method, Owner);
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
  case Decl::CXXStmtFragment:
    return InjectCXXStmtFragmentDecl(cast<CXXStmtFragmentDecl>(D));
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

  // A required declarator is created as a temporary during parsing.
  // Its analogue already exists in the injectee; don't inject it.
  if (isa<DeclaratorDecl>(D))
    if (cast<DeclaratorDecl>(D)->isRequired())
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
  if (R->getDeclContext()->isFileContext() && !MockInjectionContext)
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

  Decl *ND = MockInjectDecl(D->getFriendDecl());

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

static bool isMetaDecl(Decl *D) {
  return isa<CXXInjectorDecl>(D);
}
  
static bool isRequiresDecl(DeclStmt *DS) {
  if (!DS->isSingleDecl())
    return false;

  return isRequiresDecl(DS->getSingleDecl());
}

static Decl *AddDeclToInjecteeScope(InjectionContext &Ctx, Decl *OldDecl) {
  Sema &SemaRef = Ctx.getSema();

  Decl *D = Ctx.InjectDecl(OldDecl);
  if (!D || D->isInvalidDecl())
    return nullptr;

  if (isRequiresDecl(D))
    return nullptr;
  if (isa<CXXMetaprogramDecl>(D) || isa<CXXInjectionDecl>(D))
    return D;

  // Add the declaration to scope, we don't need to add it to the context,
  // as this should have been handled by the injection of the decl.
  Scope *FunctionScope =
    SemaRef.getScopeForContext(Decl::castToDeclContext(Ctx.Injectee));
  SemaRef.PushOnScopeChains(cast<NamedDecl>(D), FunctionScope,
                            /*AddToContext=*/false);

  return D;
}

Stmt *InjectionContext::InjectDeclStmt(DeclStmt *S) {
  llvm::SmallVector<Decl *, 4> Decls;
  unsigned IsRequiredDeclaration = false;
  for (Decl *D : S->decls()) {
    if (Decl *NewDecl = AddDeclToInjecteeScope(*this, D)) {
      IsRequiredDeclaration |= isRequiresDecl(NewDecl);

      Decls.push_back(NewDecl);
      if (isMetaDecl(NewDecl)) {
        CXXInjectorDecl *MetaDecl = cast<CXXInjectorDecl>(NewDecl);
        PushInjectedStmt(MetaDecl, InjectedStmts);
      }
    } else
      return nullptr;
  }

  StmtResult Res = RebuildDeclStmt(Decls, S->getBeginLoc(), S->getEndLoc());
  if (Res.isInvalid())
    return nullptr;

  DeclStmt *NewStmt = cast<DeclStmt>(Res.get());
  if (!isRequiresDecl(NewStmt))
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

Decl *InjectionContext::InjectTemplateTypeParmDecl(TemplateTypeParmDecl *D) {
  TemplateTypeParmDecl *Parm = TemplateTypeParmDecl::Create(
      getSema().Context, getSema().CurContext, D->getBeginLoc(), D->getLocation(),
      D->getDepth(), D->getIndex(), D->getIdentifier(),
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
      D->getLocation(), D->getDepth(), D->getPosition(), D->getIdentifier(),
      T, D->isParameterPack(), DI);
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
      getSema().Context, getSema().CurContext, D->getLocation(), D->getDepth(),
      D->getPosition(), D->isParameterPack(), D->getIdentifier(), Params);
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

  for (unsigned I = 0; I < MetaDecl->getNumInjectedDecls(); ++I) {
    Decl *ECD = MetaDecl->getInjectedDecls()[I];
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

  Enum->setInstantiationOfMemberEnum(D, TSK_ImplicitInstantiation);
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

Decl *InjectionContext::InjectCXXStmtFragmentDecl(CXXStmtFragmentDecl *D) {
  if (MockInjectionContext) {
    CXXStmtFragmentDecl *SFD =
      CXXStmtFragmentDecl::Create(getContext(), SemaRef.CurContext,
                                  D->getBeginLoc());
    SFD->setBody(D->getBody());
    return SFD;
  }

  return nullptr;
}

/// Creates the mapping between the CXXRequiredTypeDecl *D,
/// and the corresponding type.
///
/// Returns true on error.
static bool CXXRequiredTypeDeclTypeSubst(InjectionContext &Ctx,
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
                   << FD << FD->isConstexpr();
      S.Diag(FoundFD->getLocation(), diag::note_previous_declaration);
      return true;
    }

    if (FD->getReturnType()->getContainedAutoType()) {
      if (S.TypeCheckRequiredAutoReturn(FD->getLocation(),
            FD->getReturnType(), FoundFD->getReturnType()))
        return true;
      DiagnoseTypeMismatch =
        !S.Context.hasSameFunctionTypeIgnoringReturn(RDDTy, FoundDeclTy);
    } else {
      // There may be default arguments in the required declarator declaration,
      // so we can only really check for a return type mismatch here.
      DiagnoseTypeMismatch = !S.Context.hasSameType(FD->getReturnType(),
                                                    FoundFD->getReturnType());
    }
  }
  if (const VarDecl *FoundVD = dyn_cast<VarDecl>(FoundDeclarator)) {
    VarDecl *VD = cast<VarDecl>(RequiredDeclarator);
    if (VD->isConstexpr() != FoundVD->isConstexpr()) {
      S.Diag(RDLoc, diag::err_constexpr_redecl_mismatch)
        << VD << VD->isConstexpr();
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

// Create a this pointer without any qualifiers.
static QualType createFakeThisPtr(CXXRecordDecl *D) {
  ASTContext &C = D->getASTContext();
  QualType ClassType = C.getTypeDeclType(D);
  return C.getPointerType(ClassType);
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
  ExprResult CallRes;
  if (isa<CXXRecordDecl>(D->getDeclContext())) {
    CXXScopeSpec SS;
    DeclContext *InjecteeAsDC = Decl::castToDeclContext(Ctx.Injectee);
    Scope *S = SemaRef.getScopeForContext(InjecteeAsDC);
    // Use the parsed scope if it is available. If not, look it up.
 
    ParserLookupSetup ParserLookup(SemaRef, SemaRef.CurContext);
    if (!S)
      S = ParserLookup.getCurScope();

    // Create a this pointer for the injectee.
    // FIXME: We haven't selected the overload yet, so we don't
    // know the qualifiers on the this type. Also, we need to check
    // for static functions.
    QualType ThisType = createFakeThisPtr(cast<CXXRecordDecl>(Ctx.Injectee));
    SemaRef.CXXThisTypeOverride = ThisType;
    ExprResult IME =
      SemaRef.BuildPossibleImplicitMemberExpr(SS, SourceLocation(), R,
                                              nullptr, S);
    CallRes = SemaRef.ActOnCallExpr(nullptr, IME.get(), SourceLocation(),
                                    Params, SourceLocation());
  } else {
    const UnresolvedSetImpl &FoundNames = R.asUnresolvedSet();
    UnresolvedLookupExpr *ULE =
      UnresolvedLookupExpr::Create(SemaRef.Context, nullptr,
                                  D->getRequiredDeclarator()->getQualifierLoc(),
                                   D->getRequiredDeclarator()->getNameInfo(),
                                   /*ADL=*/true, /*Overloaded=*/true,
                                   FoundNames.begin(), FoundNames.end());
    CallRes = SemaRef.ActOnCallExpr(nullptr, ULE, SourceLocation(),
                                    Params, SourceLocation());
  }

  if (CallRes.isInvalid()) {
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
static bool CXXRequiredDeclaratorDeclSubst(InjectionContext &Ctx,
                                           LookupResult &R,
                                           CXXRequiredDeclaratorDecl *D) {
  Sema &SemaRef = Ctx.getSema();
  NamedDecl *FoundDecl = nullptr;
  if (R.isSingleResult()) {
    FoundDecl = R.getFoundDecl();

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
static bool CXXRequiredDeclSubstitute(InjectionContext &Ctx,
                                      Decl *D) {
  assert(isa<CXXRequiredDeclaratorDecl>(D) || isa<CXXRequiredTypeDecl>(D));
  Sema &SemaRef = Ctx.getSema();

  DeclContext *InjecteeAsDC = Decl::castToDeclContext(Ctx.Injectee);
  Scope *S = SemaRef.getScopeForContext(InjecteeAsDC);
  // Use the parsed scope if it is available. If not, look it up.
  ParserLookupSetup ParserLookup(SemaRef, SemaRef.CurContext);
  if (!S)
    S = ParserLookup.getCurScope();

  if (isa<CXXRequiredTypeDecl>(D)) {
    LookupResult R(SemaRef, cast<NamedDecl>(D)->getDeclName(),
                   D->getLocation(), Sema::LookupOrdinaryName);
    if (SemaRef.LookupName(R, S)) {
      return CXXRequiredTypeDeclTypeSubst(Ctx, R, cast<CXXRequiredTypeDecl>(D));
    } else {
      SemaRef.Diag(D->getLocation(), diag::err_required_name_not_found) << 0;
      return true;
    }
  } else if (isa<CXXRequiredDeclaratorDecl>(D)) {
    DeclarationName DeclName =
     cast<CXXRequiredDeclaratorDecl>(D)->getRequiredDeclarator()->getDeclName();
    LookupResult R(SemaRef, DeclName, D->getLocation(),
                   Sema::LookupOrdinaryName);
    if (SemaRef.LookupName(R, S)) {
      return CXXRequiredDeclaratorDeclSubst(Ctx, R,
                                            cast<CXXRequiredDeclaratorDecl>(D));
    } else {
      SemaRef.Diag(D->getLocation(), diag::err_required_name_not_found) << 1;
      return true;
    }
  } else
    llvm_unreachable("Unknown required declaration.");
}

template<typename DeclType>
static void
SubstituteOrMaintainRequiredDecl(InjectionContext &Ctx, DeclContext *Owner,
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
  if (!Ctx.MockInjectionContext) {
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
  SubstituteOrMaintainRequiredDecl(*this, Owner, RTD);

  return RTD;
}

Decl *
InjectionContext::InjectCXXRequiredDeclaratorDecl(CXXRequiredDeclaratorDecl *D) {
  DeclContext *Owner = getSema().CurContext;

  /// FIXME: does the declarator ever need transformed?
  CXXRequiredDeclaratorDecl *RDD =
    CXXRequiredDeclaratorDecl::Create(SemaRef.Context, Owner,
                                      D->getRequiredDeclarator(),
                                      D->getRequiresLoc());
  AddDeclSubstitution(D, RDD);
  SubstituteOrMaintainRequiredDecl(*this, Owner, RDD);

  return RDD;
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
  // An injection stmt can only appear in constexpr contexts
  if (!CurContext->isConstexprContext()) {
    Diag(Loc, diag::err_injection_stmt_constexpr);
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
  if (DC->isFunctionOrMethod() || DC->isStatementFragment())
    return 0;
  else if (DC->isRecord())
    return 1;
  else if (DC->isEnum())
    return 2;
  else if (DC->isFileContext())
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

  auto BlockTest = [] (DeclContext *DC) -> bool {
    return DC->isFunctionOrMethod() || DC->isStatementFragment();
  };
  if (Check(BlockTest))
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

static bool InjectStmtFragment(Sema &S,
                               CXXInjectorDecl *MD,
                               Decl *Injection,
                               const SmallVector<InjectionCapture, 8> &Captures,
                               Decl *Injectee) {
  return BootstrapInjection(S, Injectee, Injection, [&](InjectionContext *Ctx) {
    Ctx->MaybeAddDeclSubstitution(Injection, Injectee);
    Ctx->AddPlaceholderSubstitutions(Injection->getDeclContext(), Captures);

    CXXStmtFragmentDecl *InjectionSFD = cast<CXXStmtFragmentDecl>(Injection);
    CompoundStmt *FragmentBlock = cast<CompoundStmt>(InjectionSFD->getBody());
    for (Stmt *S : FragmentBlock->body()) {
      Stmt *Inj = Ctx->InjectStmt(S);

      if (!Inj) {
        Injectee->setInvalidDecl(true);
        continue;
      }
    }

    unsigned NumInjectedStmts = Ctx->InjectedStmts.size();
    Stmt **Stmts = new (S.Context) Stmt *[NumInjectedStmts];
    std::copy(Ctx->InjectedStmts.begin(), Ctx->InjectedStmts.end(), Stmts);
    MD->setInjectedStmts(Stmts, NumInjectedStmts);
  });
}

static bool InjectDeclFragment(Sema &S,
                               CXXInjectorDecl *MD,
                               Decl *Injection,
                               const SmallVector<InjectionCapture, 8> &Captures,
                               Decl *Injectee) {
  return BootstrapInjection(S, Injectee, Injection, [&](InjectionContext *Ctx) {
    // Setup substitutions
    Ctx->MaybeAddDeclSubstitution(Injection, Injectee);
    Ctx->AddPlaceholderSubstitutions(Injection->getDeclContext(), Captures);

    // Inject each declaration in the fragment.
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

    unsigned NumInjectedDecls = Ctx->InjectedDecls.size();
    Decl **Decls = new (S.Context) Decl *[NumInjectedDecls];
    std::copy(Ctx->InjectedDecls.begin(), Ctx->InjectedDecls.end(), Decls);
    MD->setInjectedDecls(Decls, NumInjectedDecls);
  });
}

/// Inject a fragment into the current context.
static bool InjectFragment(Sema &S,
                           CXXInjectorDecl *MD,
                           Decl *Injection,
                           const SmallVector<InjectionCapture, 8> &Captures,
                           Decl *Injectee) {
  SourceLocation POI = MD->getSourceRange().getEnd();

  DeclContext *InjectionAsDC = Decl::castToDeclContext(Injection);
  DeclContext *InjecteeAsDC = Decl::castToDeclContext(Injectee);

  if (!CheckInjectionContexts(S, POI, InjectionAsDC, InjecteeAsDC))
    return false;

  Sema::ContextRAII Switch(S, InjecteeAsDC, isa<CXXRecordDecl>(Injectee));

  // The logic for block fragments is different, since everything in the fragment
  // is stored in a CompoundStmt.
  if (isa<CXXStmtFragmentDecl>(Injection))
    return InjectStmtFragment(S, MD, Injection, Captures, Injectee);
  else
    return InjectDeclFragment(S, MD, Injection, Captures, Injectee);
}

// Inject a reflected base specifier into the current context.
static bool AddBase(Sema &S, CXXInjectorDecl *MD,
                    CXXBaseSpecifier *Injection, Decl *Injectee) {
  // SourceLocation POI = MD->getSourceRange().getEnd();
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

static bool ApplyFragmentInjection(Sema &S, CXXInjectorDecl *MD,
                                   InjectionEffect &IE, Decl *Injectee) {
  Decl *Injection = const_cast<Decl *>(GetFragInjectionDecl(S, IE));
  SmallVector<InjectionCapture, 8> &&Captures = GetFragCaptures(IE);
  return InjectFragment(S, MD, Injection, Captures, Injectee);
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
ApplyReflectionBaseInjection(Sema &S, CXXInjectorDecl *MD,
                             const Reflection &Refl, Decl *Injectee) {
  CXXBaseSpecifier *Injection = GetInjectionBase(Refl);

  return AddBase(S, MD, Injection, Injectee);
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

  // FIXME: We need to validate the Injection is compatible
  // with the Injectee.

  if (IE.ExprType->isFragmentType()) {
    return ApplyFragmentInjection(*this, MD, IE, Injectee);
  }

  // Type checking should gauarantee that the type of
  // our injection is either a Fragment or reflection.
  // Since we failed the fragment check, we must have
  // a reflection.
  assert(IE.ExprType->isReflectionType());

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

  if (D->isCXXClassMember()) {
    auto *ParentClass = cast<CXXRecordDecl>(D->getDeclContext());
    if (!ParentClass->isCompleteDefinition())
      return false;
  }

  return true;
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
}

void Sema::InjectPendingFriendFunctionDefinitions() {
  for (auto &&Ctx : PendingClassMemberInjections) {
    InjectPendingFriendFunctionDefinitions(Ctx);
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

static void InjectPendingDefinition(InjectionContext *Ctx,
                                    FunctionDecl *OldFunction,
                                    FunctionDecl *NewFunction) {
  InjectFunctionDefinition(Ctx, OldFunction, NewFunction);
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
  Sema::CodeInjectionTracker InjectingCode(Ctx->getSema());

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
    CheckCompletedCXXClass(Class);

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
                                           Declarator &D,
                                           AccessSpecifier AS) {
  // We don't want to check for linkage, memoize that we're
  // working on a required declarator for later checks.
  AnalyzingRequiredDeclarator = true;
  Decl *Dclrtr = nullptr;

  if (CurContext->isRecord() || CurContext->getParent()->isRecord()) {
    MultiTemplateParamsArg Args;
    VirtSpecifiers VS;

    Dclrtr = ActOnCXXMemberDeclarator(CurScope, AS, D, Args,
                                      nullptr, VS, ICIS_NoInit);
  } else
    Dclrtr = ActOnDeclarator(CurScope, D);
  if (!Dclrtr)
    return nullptr;
  DeclaratorDecl *DDecl = cast<DeclaratorDecl>(Dclrtr);
  AnalyzingRequiredDeclarator = false;

  if (!DDecl)
    return nullptr;
  DDecl->setRequired();
  DDecl->setAccess(AS);

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

  if (!RDD || RDD->isInvalidDecl())
    return nullptr;
  if (isa<CXXRecordDecl>(CurContext))
    CurContext->addDecl(RDD);

  return RDD;
}
