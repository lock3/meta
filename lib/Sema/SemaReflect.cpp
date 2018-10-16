//===--- SemaReflect.cpp - Semantic Analysis for Reflection ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements semantic analysis for reflection.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTContext.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/Decl.h"
#include "clang/Basic/PartialDiagnostic.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Sema/Lookup.h"
#include "clang/Sema/Scope.h"
#include "clang/Sema/SemaInternal.h"
#include "TypeLocBuilder.h"
using namespace clang;
using namespace clang::reflect;
using namespace sema;

/// Lookup the declaration named by SS and Id. Populates the the kind
/// and entity with the encoded reflection of the named entity.
bool Sema::ActOnReflectedId(CXXScopeSpec &SS, SourceLocation IdLoc,
                            IdentifierInfo *Id, unsigned &Kind,
                            ParsedReflectionPtr &Entity) {
  
  // Perform any declaration having the given name.
  LookupResult R(*this, Id, IdLoc, LookupAnyName);
  LookupParsedName(R, CurScope, &SS);

  
  if (!R.isSingleResult()) {
    // FIXME: Incorporate the scope specifier in the diagnostics. Also note
    // alternatives for an ambiguous lookup.
    //
    // FIXME: I believe that we would eventually like to support overloaded
    // declarations and templates.
    if (R.isAmbiguous())
      Diag(IdLoc, diag::err_reflect_ambiguous_id) << Id;
    else if (R.isOverloadedResult())
      Diag(IdLoc, diag::err_reflect_overloaded_id) << Id;
    else
      Diag(IdLoc, diag::err_reflect_undeclared_id) << Id;
    Kind = REK_special;
    Entity = nullptr;
    return false;
  }

  // Reflect the named entity.
  Entity = R.getAsSingle<NamedDecl>();
  Kind = REK_declaration;
  return true;
}

bool Sema::ActOnReflectedDependentId(CXXScopeSpec &SS, SourceLocation IdLoc,
                                     IdentifierInfo *Id, unsigned &Kind,
                                     ParsedReflectionPtr &Entity) {
  LookupResult R(*this, Id, IdLoc, LookupAnyName);
  LookupParsedName(R, CurScope, &SS, false, true);
  DeclarationNameInfo DNI = R.getLookupNameInfo();

  if (!R.isSingleResult()) {
    // FIXME: Incorporate the scope specifier in the diagnostics. Also note
    // alternatives for an ambiguous lookup.
    //
    // FIXME: I believe that we would eventually like to support overloaded
    // declarations and templates.
    if (R.isAmbiguous())
      Diag(IdLoc, diag::err_reflect_ambiguous_id) << Id;
    else if (R.isOverloadedResult())
      Diag(IdLoc, diag::err_reflect_overloaded_id) << Id;
    else
      Diag(IdLoc, diag::err_reflect_undeclared_id) << Id;
    Kind = REK_special;
    Entity = nullptr;
    return false;
  }

  const auto &decls = R.asUnresolvedSet();

  Entity =
    UnresolvedLookupExpr::Create(Context, /*NamingClass=*/ nullptr,
                                 SS.getWithLocInContext(Context),
                                 DNI, /*ADL=*/false, /*Overloaded=*/false,
                                 /*Reflection=*/true,
                                 decls.begin(), decls.end());
  Kind = REK_statement;
  return true;
}

/// Populates the kind and entity with the encoded reflection of the type.
/// Reflections of user-defined types are handled as entities.
bool Sema::ActOnReflectedType(Declarator &D, unsigned &Kind,
                              ParsedReflectionPtr &Entity) {
  TypeSourceInfo *TSI = GetTypeForDeclarator(D, CurScope);
  QualType T = TSI->getType();
  // FIXME: There are other types that can be declarations.
  if (TagDecl *TD = T->getAsTagDecl()) {
    // Handle elaborated type specifiers as if they were declarations.
    Entity = TD;
    Kind = REK_declaration;
  } else {
    // Otherwise, this is a type reflection.
    Entity = const_cast<Type*>(T.getTypePtr());
    Kind = REK_type;
  }
  return true;
}

/// Returns a constant expression that encodes the value of the reflection.
/// The type of the reflection is meta::reflection, an enum class.
ExprResult Sema::ActOnCXXReflectExpression(SourceLocation KWLoc,
                                           unsigned Kind,
                                           ParsedReflectionPtr Entity,
                                           SourceLocation LPLoc,
                                           SourceLocation RPLoc) {
  ReflectionKind REK = (ReflectionKind)Kind;
  APValue Reflection(REK, Entity);

  bool IsValueDependent = false;
  if (const Type* T = getAsReflectedType(Reflection)) {
    // A type reflection is dependent if reflected T is dependent.
    IsValueDependent = T->isDependentType();
  } else if (const Decl *D = getAsReflectedDeclaration(Reflection)) {
    if (const ValueDecl *VD = dyn_cast<ValueDecl>(D)) {
      // A declaration reflection is value dependent if an id-expression
      // referring to that declaration is type or value dependent. Reflections
      // of non-value declarations (e.g., namespaces) are never dependent.
      // Build a fake id-expression in order to determine dependence.
      Expr *E = new (Context) DeclRefExpr(const_cast<ValueDecl*>(VD), false,
                                          VD->getType(), VK_RValue, KWLoc);
      IsValueDependent = E->isTypeDependent() || E->isValueDependent();
    } else if (const TypeDecl *TD = dyn_cast<TypeDecl>(D)) {
      // A reflection of a type declaration is dependent if that type is
      // dependent.
      const Type* T = TD->getTypeForDecl();
      IsValueDependent = T->isDependentType();
    }
  } else if (const UnresolvedLookupExpr *ULE = getAsReflectedULE(Reflection)) {
    IsValueDependent = true;
  }

  return CXXReflectExpr::Create(Context, KWLoc, Context.MetaInfoTy, Reflection,
                                LPLoc, RPLoc, VK_RValue,
                                /*isTypeDependent=*/false,
                                /*isValueDependent=*/IsValueDependent,
                                /*isInstDependent=*/IsValueDependent,
                                /*containsUnexpandedPacks=*/false);
}

// Convert each operand to an rvalue.
static void ConvertTraitOperands(Sema &SemaRef, ArrayRef<Expr *> Args,
                               SmallVectorImpl<Expr *> &Operands) {
  for (std::size_t I = 0; I < Args.size(); ++I) {
    if (Args[I]->isGLValue())
      Operands[I] = ImplicitCastExpr::Create(SemaRef.Context,
                                             Args[I]->getType(),
                                             CK_LValueToRValue, Args[I],
                                             nullptr, VK_RValue);
    else
      Operands[I] = Args[I];
  }
}

// Check that the argument has the right type. Ignore references and
// cv-qualifiers on the expression.
static bool CheckReflectionOperand(Sema &SemaRef, Expr *E) {
  // Get the type of the expression.
  QualType Source = E->getType();
  if (Source->isReferenceType())
    Source = Source->getPointeeType();
  Source = Source.getUnqualifiedType();
  Source = SemaRef.Context.getCanonicalType(Source);

  // FIXME: We should cache meta::info and simply compare against that.
  if (Source != SemaRef.Context.MetaInfoTy) {
    SemaRef.Diag(E->getBeginLoc(), diag::err_reflection_trait_wrong_type)
        << Source;
    return false;
  }

  return true;
}

ExprResult Sema::ActOnCXXReflectionTrait(SourceLocation TraitLoc,
                                         ReflectionTrait Trait,
                                         ArrayRef<Expr *> Args,
                                         SourceLocation RParenLoc) {
  // If any arguments are dependent, then the expression is dependent.
  for (std::size_t I = 0; I < Args.size(); ++I) {
    Expr *Arg = Args[0];
    if (Arg->isTypeDependent() || Arg->isValueDependent())
      return new (Context) CXXReflectionTraitExpr(Context, Context.DependentTy,
                                                  Trait, TraitLoc, Args,
                                                  RParenLoc);
  }

  // Build a set of converted arguments.
  SmallVector<Expr *, 2> Operands(Args.size());
  ConvertTraitOperands(*this, Args, Operands);

  // Check the type of the first operand. Note: ReflectPrint is polymorphic.
  if (Trait != URT_ReflectPrint) {
    if (!CheckReflectionOperand(*this, Operands[0]))
      return false;
  }

  // FIXME: If the trait allows multiple arguments, check those.
  QualType ResultTy;
  switch (Trait) {
    case URT_ReflectIndex: // meta::reflection_kind
      ResultTy = Context.IntTy;
      break;

    case URT_ReflectContext:
    case URT_ReflectHome:
    case URT_ReflectBegin:
    case URT_ReflectEnd:
    case URT_ReflectNext:
    case URT_ReflectType: // meta::info
      ResultTy = Context.MetaInfoTy;
      break;

    case URT_ReflectName: // const char*
      ResultTy = Context.getPointerType(Context.CharTy.withConst());
      break;

    case URT_ReflectTraits: // unsigned
      ResultTy = Context.UnsignedIntTy;
      break;

    case URT_ReflectPrint: // int (always 0)
      // Returns 0.
      ResultTy = Context.IntTy;
      break;
  }
  assert(!ResultTy.isNull() && "unknown reflection trait");

  return new (Context) CXXReflectionTraitExpr(Context, ResultTy, Trait,
                                              TraitLoc, Operands, RParenLoc);
}

ExprResult Sema::ActOnCXXReflectedValueExpression(SourceLocation Loc,
                                                  Expr *Reflection) {
  return BuildCXXReflectedValueExpression(Loc, Reflection);
}

ExprResult Sema::BuildCXXReflectedValueExpression(SourceLocation Loc,
                                                  Expr *E) {
  // Don't act on dependent expressions, just preserve them.
  if (E->isTypeDependent() || E->isValueDependent())
    return new (Context) CXXReflectedValueExpr(E, Context.DependentTy,
                                               VK_RValue, OK_Ordinary, Loc);

  // The operand must be a reflection.
  if (!CheckReflectionOperand(*this, E))
    return ExprError();

  // Evaluate the reflection.
  SmallVector<PartialDiagnosticAt, 4> Diags;
  Expr::EvalResult Result;
  Result.Diag = &Diags;
  if (!E->EvaluateAsRValue(Result, Context)) {
    Diag(E->getExprLoc(), diag::reflection_not_constant_expression);
    for (PartialDiagnosticAt PD : Diags)
      Diag(PD.first, PD.second);
    return ExprError();
  }

  APValue Reflection = Result.Val;
  if (isNullReflection(Reflection) || isReflectedType(Reflection)) {
    // FIXME: This is the wrong error.
    Diag(E->getExprLoc(), diag::err_expression_not_value_reflection);
    return ExprError();
  }

  // Build a declaration reference that would refer to the reflected entity.
  Decl* D = const_cast<Decl*>(getReflectedDeclaration(Reflection));
  if (!isa<ValueDecl>(D)) {
    Diag(E->getExprLoc(), diag::err_expression_not_value_reflection);
    return ExprError();
  }
  ValueDecl *VD = cast<ValueDecl>(D);

  // FIXME: We shouldn't be able to generate addresses for constructors,
  // destructors, or local variables.

  // Build a reference to the declaration.
  CXXScopeSpec SS;
  DeclarationNameInfo DNI(VD->getDeclName(), VD->getLocation());
  ExprResult ER = BuildDeclarationNameExpr(SS, DNI, VD);
  if (ER.isInvalid())
    return ExprError();
  Expr *Ref = ER.get();

  // For data members and member functions, adjust the expression so that
  // we evaluate a pointer-to-member, not the member itself.
  if (FieldDecl *FD = dyn_cast<FieldDecl>(VD)) {
    const Type *C = Context.getTagDeclType(FD->getParent()).getTypePtr();
    QualType PtrTy = Context.getMemberPointerType(FD->getType(), C);
    Ref = new (Context) UnaryOperator(Ref, UO_AddrOf, PtrTy, VK_RValue,
                                      OK_Ordinary, Loc, false);
  } else if (CXXMethodDecl *MD = dyn_cast<CXXMethodDecl>(VD)) {
    const Type *C = Context.getTagDeclType(MD->getParent()).getTypePtr();
    QualType PtrTy = Context.getMemberPointerType(MD->getType(), C);
    Ref = new (Context) UnaryOperator(Ref, UO_AddrOf, PtrTy, VK_RValue,
                                      OK_Ordinary, Loc, false);
  }

  // The type and category of the expression are those of an id-expression
  // denoting the reflected entity.
  CXXReflectedValueExpr *Val =
      new (Context) CXXReflectedValueExpr(E, Ref->getType(), Ref->getValueKind(),
                                          Ref->getObjectKind(), Loc);
  Val->setReference(Ref);
  return Val;
}

/// Evaluates the given expression and yields the computed type.
TypeResult Sema::ActOnReflectedTypeSpecifier(SourceLocation TypenameLoc,
                                             Expr *E) {
  QualType T = BuildReflectedType(TypenameLoc, E);
  if (T.isNull())
    return TypeResult(true);

  // FIXME: Add parens?
  TypeLocBuilder TLB;
  ReflectedTypeLoc TL = TLB.push<ReflectedTypeLoc>(T);
  TL.setNameLoc(TypenameLoc);
  TypeSourceInfo *TSI = TLB.getTypeSourceInfo(Context, T);
  return CreateParsedType(T, TSI);
}

/// Evaluates the given expression and yields the computed type.
QualType Sema::BuildReflectedType(SourceLocation TypenameLoc, Expr *E) {
  if (E->isTypeDependent() || E->isValueDependent())
    return Context.getReflectedType(E, Context.DependentTy);

  // The operand must be a reflection.
  if (!CheckReflectionOperand(*this, E))
    return QualType();

  // Evaluate the reflection.
  SmallVector<PartialDiagnosticAt, 4> Diags;
  Expr::EvalResult Result;
  Result.Diag = &Diags;
  if (!E->EvaluateAsRValue(Result, Context)) {
    Diag(E->getExprLoc(), diag::reflection_not_constant_expression);
    for (PartialDiagnosticAt PD : Diags)
      Diag(PD.first, PD.second);
    return QualType();
  }

  APValue Reflection = Result.Val;
  if (isNullReflection(Reflection)) {
    Diag(E->getExprLoc(), diag::err_empty_type_reflection);
    return QualType();
  }

  // Get the type of the reflected entity.
  QualType Reflected;
  if (const Type* T = getAsReflectedType(Reflection)) {
    Reflected = QualType(T, 0);
  } else if (const Decl* D = getAsReflectedDeclaration(Reflection)) {
    if (const TypeDecl *TD = dyn_cast<TypeDecl>(D)) {
      Reflected = Context.getTypeDeclType(TD);
    } else {
      Diag(E->getExprLoc(), diag::err_expression_not_type_reflection);
      return QualType();
    }
  } else {
    // FIXME: Handle things like base classes.
    llvm_unreachable("unknown reflection");
  }

  return Context.getReflectedType(E, Reflected);
}


