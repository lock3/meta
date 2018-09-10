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

/// Records information about a definition inside a fragment that must be
/// processed later. These are typically fields and methods.
struct InjectedDef {
  InjectedDef(Decl *F, Decl *I) : Fragment(F), Injected(I) { }

  /// The declaration within the fragment.
  Decl *Fragment;

  /// The injected declaration.
  Decl *Injected;
};

class InjectionContext;

/// \brief An injection context. This is declared to establish a set of
/// substitutions during an injection.
class InjectionContext : public TreeTransform<InjectionContext> {
   using Base = TreeTransform<InjectionContext>;
public:
  InjectionContext(Sema &SemaRef) : Base(SemaRef) { }

  ASTContext &getContext() { return getSema().Context; }

  /// Detach the context from the semantics object. Returns this object for
  /// convenience.
  InjectionContext *Detach() {
    return this;
  }

  /// Re-attach the context to the context stack.
  void Attach() {
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

  /// Returns true if D is within an injected fragment or cloned declaration.
  bool IsInInjection(Decl *D);

  DeclarationNameInfo TransformDeclarationName(NamedDecl *ND) {
    DeclarationNameInfo DNI(ND->getDeclName(), ND->getLocation());
    return TransformDeclarationNameInfo(DNI);
  }

  bool InjectDeclarator(DeclaratorDecl *D, DeclarationNameInfo &DNI,
		   TypeSourceInfo *&TSI);
  void UpdateFunctionParms(FunctionDecl* Old, FunctionDecl* New);
  bool InjectMemberDeclarator(DeclaratorDecl *D, DeclarationNameInfo &DNI,
                              TypeSourceInfo *&TSI, CXXRecordDecl *&Owner);
  Decl *InjectCXXMethodDecl(CXXMethodDecl *D);
  Decl *InjectDeclImpl(Decl *D);
  Decl *InjectDecl(Decl *D);

  // Members

  /// \brief A list of declarations whose definitions have not yet been
  /// injected. These are processed when a class receiving injections is
  /// completed.
  llvm::SmallVector<InjectedDef, 8> InjectedDefinitions;
};

bool InjectionContext::IsInInjection(Decl *D) {
  return true;
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
      ParmVarDecl *NewParm = NewParms[NewIndex++];
      NewParm->setOwningFunction(New);
    } while (OldIndex < OldParms.size() && NewIndex < NewParms.size());
  } else {
    assert(NewParms.size() == 0);
  }
  assert(OldIndex == OldParms.size() && NewIndex == NewParms.size());
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
  UpdateFunctionParms(D, Method);

  // Propagate semantic properties.
  Method->setImplicit(D->isImplicit());
  Method->setAccess(D->getAccess());

  // Propagate virtual flags.
  Method->setVirtualAsWritten(D->isVirtualAsWritten());
  if (D->isPure())
    SemaRef.CheckPureMethod(Method, Method->getSourceRange());

  Method->setDeletedAsWritten(D->isDeletedAsWritten());
  Method->setDefaulted(D->isDefaulted());

  if (!Method->isInvalidDecl())
    Method->setInvalidDecl(Invalid);

  // Don't register the declaration if we're injecting the declaration of
  // a template-declaration. We'll add the template later.
  if (!D->getDescribedFunctionTemplate())
    Owner->addDecl(Method);

  // If the method is has a body, add it to the context so that we can
  // process it later. Note that deleted/defaulted definitions are just
  // flags processed above. Ignore the definition if we've marked this
  // as pure virtual.
  if (D->hasBody() && !Method->isPure())
    InjectedDefinitions.push_back(InjectedDef(D, Method));

  return Method;
}

Decl *InjectionContext::InjectDeclImpl(Decl *D) {
  // Inject the declaration.
  switch (D->getKind()) {
  case Decl::CXXMethod:
  case Decl::CXXConstructor:
  case Decl::CXXDestructor:
  case Decl::CXXConversion:
    return InjectCXXMethodDecl(cast<CXXMethodDecl>(D));
  default:
    break;
  }
  D->dump();
  llvm_unreachable("unhandled declaration");
}

/// \brief Injects a new version of the declaration. Do not use this to
/// resolve references to declarations; use ResolveDecl instead.
Decl *InjectionContext::InjectDecl(Decl *D) {
  assert(!GetDeclReplacement(D) && "Declaration already injected");

  // If the declaration does not appear in the context, then it need
  // not be resolved.
  if (!IsInInjection(D))
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

} // namespace clang

/// Called at the start of a source code fragment to establish the fragment
/// declaration and placeholders.
Decl *Sema::ActOnStartCXXFragment(Scope* S, SourceLocation Loc) {
  CXXFragmentDecl *Fragment = CXXFragmentDecl::Create(Context, CurContext, Loc);

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
ExprResult Sema::ActOnCXXFragmentExpr(SourceLocation Loc, Decl *Fragment) {
  return BuildCXXFragmentExpr(Loc, Fragment);
}

/// \brief Builds a new fragment expression.
/// Consider the following:
///
///   constexpr {
///     auto x = <<class: int a, b, c;>>;
///   }
///
/// The type of the expression is a new meta:: class defined, approximately,
/// like this:
///
///   using typename(reflexpr(<fragment>)) = refl_type;
///
///   struct __fragment_type  {
///     refl_type fragment_reflection;
///
///     __fragment_type(refl_type fragment_reflection) :
///        fragment_reflection(fragment_reflection) { }
///   };
///
ExprResult Sema::BuildCXXFragmentExpr(SourceLocation Loc, Decl *Fragment) {
  CXXFragmentDecl *FD = cast<CXXFragmentDecl>(Fragment);

  // Reflection; need to get a ReflectExpr the Fragment
  // hold as a static consptexpr std::meta::info on the generated class
  // create a default constructor so that the fragment can
  // be initialized.

  // If the fragment appears in a context that depends on template parameters,
  // then the expression is dependent.
  //
  // FIXME: This is just an approximation of the right answer. In truth, the
  // expression is dependent if the fragment depends on any template parameter
  // in this or any enclosing context.
  if (CurContext->isDependentContext()) {
    return new (Context) CXXFragmentExpr(Context, Loc, Context.DependentTy,
                                         FD, nullptr);
  }

  // Build the expression used to the reflection of fragment.
  //
  // TODO: We should be able to compute the type without generating an
  // expression. We're not actually using the expression.
  ExprResult Reflection = ActOnCXXReflectExpression(
    /*KWLoc=*/SourceLocation(), /*Kind=*/ReflectionKind::REK_declaration,
    /*Entity=*/FD->getContent(), /*LPLoc=*/SourceLocation(),
    /*RPLoc=*/SourceLocation());
  if (Reflection.isInvalid())
    return ExprError();

  // Build our new class implicit class to hold our fragment info.
  CXXRecordDecl *Class = CXXRecordDecl::Create(
					       Context, TTK_Class, FD, Loc, Loc,
					       /*Id=*/nullptr,
					       /*PrevDecl=*/nullptr);
  StartDefinition(Class);

  Class->setImplicit(true);
  Class->setFragment(true);

  QualType ClassTy = Context.getRecordType(Class);
  TypeSourceInfo *ClassTSI = Context.getTrivialTypeSourceInfo(ClassTy);


  // Build the class fields.
  SmallVector<FieldDecl *, 4> Fields;
  QualType ReflectionType = Reflection.get()->getType();
  IdentifierInfo *ReflectionFieldId = &Context.Idents.get(
      "fragment_reflection");
  TypeSourceInfo *ReflectionTypeInfo = Context.getTrivialTypeSourceInfo(
      ReflectionType);

  /// TODO This can be changed to a VarDecl to make it static
  /// member data
  QualType ConstReflectionType = ReflectionType.withConst();
  FieldDecl *Field = FieldDecl::Create(
                                       Context, Class, Loc, Loc, ReflectionFieldId,
                                       ConstReflectionType, ReflectionTypeInfo,
                                       nullptr, false,
                                       ICIS_NoInit);
  Field->setAccess(AS_public);
  Field->setImplicit(true);

  Fields.push_back(Field);
  Class->addDecl(Field);

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

  QualType CtorTy = Context.getFunctionType(Context.VoidTy, ArgTypes, EPI);
  Ctor->setType(CtorTy);

  // Build the constructor params.
  SmallVector<ParmVarDecl *, 4> Parms;
  IdentifierInfo *ReflectionParmId = &Context.Idents.get("fragment_reflection");
  ParmVarDecl *Parm = ParmVarDecl::Create(Context, Ctor, Loc, Loc,
                                          ReflectionParmId,
                                          ReflectionType, ReflectionTypeInfo,
                                          SC_None, nullptr);
  Parm->setScopeInfo(0, 0);
  Parm->setImplicit(true);
  Parms.push_back(Parm);

  Ctor->setParams(Parms);

  // Build constructor initializers.
  std::size_t NumInits = Fields.size();
  CXXCtorInitializer **Inits = new (Context) CXXCtorInitializer *[NumInits];

  // Build member initializers.
  for (std::size_t I = 0; I < Parms.size(); ++I) {
    ParmVarDecl *Parm = Parms[I];
    FieldDecl *Field = Fields[I];
    DeclRefExpr *Ref = new (Context) DeclRefExpr(
        Parm, false, Parm->getType(), VK_LValue, Loc);
    Expr *Arg = new (Context) ParenListExpr(Context, Loc, Ref, Loc);
    Inits[I] = BuildMemberInitializer(Field, Arg, Loc).get();
  }
  Ctor->setNumCtorInitializers(NumInits);
  Ctor->setCtorInitializers(Inits);

  // Build the definition.
  Stmt *Def = CompoundStmt::Create(Context, None, Loc, Loc);
  Ctor->setBody(Def);
  Class->addDecl(Ctor);

  CompleteDefinition(Class);

  // Setup the arguments to use for initialization.
  SmallVector<Expr *, 4> CtorArgs;
  CtorArgs.push_back(Reflection.get());

  // Build an expression that that initializes the fragment object.
  CXXConstructExpr *Cast = CXXConstructExpr::Create(
      Context, ClassTy, Loc, Ctor, true, CtorArgs,
      /*HadMultipleCandidates=*/false, /*ListInitialization=*/false,
      /*StdInitListInitialization=*/false, /*ZeroInitialization=*/false,
      CXXConstructExpr::CK_Complete, SourceRange(Loc, Loc));
  Expr *Init = CXXFunctionalCastExpr::Create(
      Context, ClassTy, VK_RValue, ClassTSI, CK_NoOp, Cast,
      /*Path=*/nullptr, Loc, Loc);

  // Finally, build the fragment expression.
  return new (Context) CXXFragmentExpr(Context, Loc, ClassTy, FD, Init);
}

/// Returns an injection statement.
StmtResult Sema::ActOnCXXInjectionStmt(SourceLocation Loc, Expr *Fragment) {
  return BuildCXXInjectionStmt(Loc, Fragment);
}

/// Returns an injection statement.
StmtResult Sema::BuildCXXInjectionStmt(SourceLocation Loc, Expr *Fragment) {
  // The operand must be a reflection (if non-dependent).
  // if (!Fragment->isTypeDependent() && !Fragment->isValueDependent()) {
  if (!Fragment->getType()->getAsCXXRecordDecl()->isFragment()) {
    Diag(Fragment->getExprLoc(), diag::err_not_a_fragment);
    return StmtError();
  }
  // }

  // Perform an lvalue-to-value conversion so that we get an rvalue in
  // evaluation.
  if (Fragment->isGLValue())
    Fragment = ImplicitCastExpr::Create(Context, Fragment->getType(),
                                        CK_LValueToRValue, Fragment,
                                        nullptr, VK_RValue);

  return new (Context) CXXInjectionStmt(Loc, Fragment);
}

// Returns an integer value describing the target context of the injection.
// This correlates to the second %select in err_invalid_injection.
static int DescribeInjectionTarget(DeclContext *DC) {
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

// Generate an error injecting a declaration of kind SK into the given
// declaration context. Returns false. Note that SK correlates to the first
// %select in err_invalid_injection.
static bool InvalidInjection(Sema& S, SourceLocation POI, int SK,
                             DeclContext *DC) {
  S.Diag(POI, diag::err_invalid_injection) << SK << DescribeInjectionTarget(DC);
  return false;
}

static bool CheckInjectionContexts(Sema &SemaRef, SourceLocation POI,
                                   DeclContext *Injection,
                                   DeclContext *Injectee) {
  if (Injection->isRecord() && !Injectee->isRecord()) {
    InvalidInjection(SemaRef, POI, 1, Injectee);
    return false;
  } else if (Injection->isFileContext() && !Injectee->isFileContext()) {
    InvalidInjection(SemaRef, POI, 0, Injectee);
    return false;
  }
  return true;
}

/// Inject a fragment into the current context.
bool Sema::InjectFragment(SourceLocation POI,
                          const Decl *Injection,
                          Decl *Injectee) {
  assert(isa<CXXRecordDecl>(Injection) || isa<NamespaceDecl>(Injection));
  DeclContext *InjectionDC = Decl::castToDeclContext(Injection);
  DeclContext *InjecteeDC = Decl::castToDeclContext(Injectee);

  if (!CheckInjectionContexts(*this, POI, InjectionDC, InjecteeDC))
    return false;

  ContextRAII Switch(*this, InjecteeDC, isa<CXXRecordDecl>(Injectee));

  // Establish the injection context and register the substitutions.
  InjectionContext *Cxt = new InjectionContext(*this);
  // Cxt->AddDeclSubstitution(Injection, Injectee);

  // Inject each declaration in the fragment.
  for (Decl *D : InjectionDC->decls()) {
    // Never inject injected class names.
    if (CXXRecordDecl *Class = dyn_cast<CXXRecordDecl>(D))
      if (Class->isInjectedClassName())
        continue;

    Decl *R = Cxt->InjectDecl(D);
    if (!R || R->isInvalidDecl()) {
      Injectee->setInvalidDecl(true);
      continue;
    }
  }

  // If we're injecting into a class and have pending definitions, attach
  // those to the class for subsequent analysis.
  if (CXXRecordDecl *ClassInjectee = dyn_cast<CXXRecordDecl>(Injectee)) {
    if (!Injectee->isInvalidDecl() && !Cxt->InjectedDefinitions.empty()) {
      PendingClassMemberInjections.push_back(Cxt->Detach());
      return true;
    }
  }

  delete Cxt;
  return !Injectee->isInvalidDecl();
}

static const Decl *
GetDeclFromReflection(Sema &SemaRef, APValue FragmentData, SourceLocation Loc) {
  assert(FragmentData.isStruct()
	 && "expected FragmentData to be a struct value");
  Reflection Refl(FragmentData.getStructField(0));
  return Refl.getDeclaration();
}

bool Sema::ApplyInjection(SourceLocation POI, InjectionInfo &II) {
  const Decl *Injection = GetDeclFromReflection(*this, II.FragmentData, POI);

  Decl *Injectee = Decl::castFromDeclContext(CurContext);
  if (!Injectee) {
    return false;
  }

  // FIXME: We need to validate the Injection is compatible
  // with the Injectee.

  return InjectFragment(POI, Injection, Injectee);
}

static void
PrintDecl(Sema &SemaRef, const Decl *D) {
  PrintingPolicy PP = SemaRef.Context.getPrintingPolicy();
  PP.TerseOutput = false;
  D->print(llvm::errs(), PP);
  llvm::errs() << '\n';
}

static void
PrintType(Sema &SemaRef, const Type *T) {
  if (TagDecl *TD = T->getAsTagDecl())
    return PrintDecl(SemaRef, TD);
  PrintingPolicy PP = SemaRef.Context.getPrintingPolicy();
  QualType QT(T, 0);
  QT.print(llvm::errs(), PP);
  llvm::errs() << '\n';
}

static bool
ApplyDiagnostic(Sema &SemaRef, SourceLocation Loc, const APValue &Arg) {
  Reflection R(Arg);
  if (const Decl *D = R.getAsDeclaration()) {
    // D->dump();
    PrintDecl(SemaRef, D);
  }
  else if (const Type *T = R.getAsType()) {
    // if (TagDecl *TD = T->getAsTagDecl())
    //   TD->dump();
    // else
    //   T->dump();
    PrintType(SemaRef, T);
  }
  else
    llvm_unreachable("printing invalid reflection");
  return true;
}

/// Inject a sequence of source code fragments or modification requests
/// into the current AST. The point of injection (POI) is the point at
/// which the injection is applied.
///
/// \returns  true if no errors are encountered, false otherwise.
bool Sema::ApplyEffects(SourceLocation POI,
                        SmallVectorImpl<EvalEffect> &Effects) {
  bool Ok = true;
  for (EvalEffect &Effect : Effects) {
    if (Effect.Kind == EvalEffect::InjectionEffect)
      Ok &= ApplyInjection(POI, *Effect.Injection);
    else
      Ok &= ApplyDiagnostic(*this, POI, *Effect.DiagnosticArg);
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
  bool is_empty = PendingClassMemberInjections.empty();
  if (is_empty)
    return false;
  InjectionContext *Cxt = PendingClassMemberInjections.back();
  assert(!Cxt->InjectedDefinitions.empty() && "bad injection queue");
  InjectedDef& Def = Cxt->InjectedDefinitions.front();
  DeclContext *DC = Def.Injected->getDeclContext();
  while (!DC->isFileContext()) {
    if (DC == D)
      return true;
    DC = DC->getParent();
  }
  return false;
}

void Sema::InjectPendingDefinitions() {
  while (!PendingClassMemberInjections.empty()) {
    InjectionContext *Cxt = PendingClassMemberInjections.back();
    PendingClassMemberInjections.pop_back();
    InjectPendingDefinitions(Cxt);
  }
}

void Sema::InjectPendingDefinitions(InjectionContext *Cxt) {
  Cxt->Attach();
  for (InjectedDef& Def : Cxt->InjectedDefinitions)
    InjectPendingDefinition(Cxt, Def.Fragment, Def.Injected);
  delete Cxt;
}

void Sema::InjectPendingDefinition(InjectionContext *Cxt,
                                   Decl *Frag,
                                   Decl *New) {
  // FIXME: Everything should already be parsed
  // this should be unncessary
  PushFunctionScope();

  // Switch to the class enclosing the newly injected declaration.
  ContextRAII ClassCxt (*this, New->getDeclContext());

  if (FieldDecl *OldField = dyn_cast<FieldDecl>(Frag)) {
    FieldDecl *NewField = cast<FieldDecl>(New);
    ExprResult Init = Cxt->TransformExpr(OldField->getInClassInitializer());
    if (Init.isInvalid())
      NewField->setInvalidDecl();
    else
      NewField->setInClassInitializer(Init.get());
  }
  else if (CXXMethodDecl *OldMethod = dyn_cast<CXXMethodDecl>(Frag)) {
    CXXMethodDecl *NewMethod = cast<CXXMethodDecl>(New);
    ContextRAII MethodCxt (*this, NewMethod);
    StmtResult Body = Cxt->TransformStmt(OldMethod->getBody());
    if (Body.isInvalid())
      NewMethod->setInvalidDecl();
    else
      NewMethod->setBody(Body.get());
  }
}
