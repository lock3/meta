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
#include "clang/AST/ASTDiagnostic.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/Decl.h"
#include "clang/Basic/PartialDiagnostic.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Sema/Lookup.h"
#include "clang/Sema/ParsedTemplate.h"
#include "clang/Sema/ParsedReflection.h"
#include "clang/Sema/ParserLookupSetup.h"
#include "clang/Sema/Scope.h"
#include "clang/Sema/ScopeInfo.h"
#include "clang/Sema/SemaInternal.h"
#include "TypeLocBuilder.h"

using namespace clang;
using namespace sema;

ParsedReflectionOperand Sema::ActOnReflectedType(TypeResult T) {
  // Cheat by funneling this back through template argument processing
  // because it's possible to end up with deduction guides.
  ParsedTemplateArgument Arg = ActOnTemplateTypeArgument(T);
  if (Arg.getKind() == ParsedTemplateArgument::Template)
    return ActOnReflectedTemplate(Arg);
  assert(Arg.getKind() == ParsedTemplateArgument::Type);

  return ParsedReflectionOperand(T.get(), Arg.getLocation());
}

ParsedReflectionOperand Sema::ActOnReflectedTemplate(ParsedTemplateArgument T) {
  assert(T.getKind() == ParsedTemplateArgument::Template);
  const CXXScopeSpec& SS = T.getScopeSpec();
  ParsedTemplateTy Temp = T.getAsTemplate();

  return ParsedReflectionOperand(SS, Temp, T.getLocation());
}

ParsedReflectionOperand Sema::ActOnReflectedNamespace(CXXScopeSpec &SS,
                                                      SourceLocation &Loc,
                                                      Decl *D) {
  return ParsedReflectionOperand(SS, D, Loc);
}

ParsedReflectionOperand Sema::ActOnReflectedNamespace(SourceLocation Loc) {
  // Clang uses TUDecl in place of having a global namespace.
  return ParsedReflectionOperand(Context.getTranslationUnitDecl(), Loc);
}

ParsedReflectionOperand Sema::ActOnReflectedExpression(Expr *E) {
  return ParsedReflectionOperand(E, E->getBeginLoc());
}

/// Returns a constant expression that encodes the value of the reflection.
/// The type of the reflection is meta::reflection, an enum class.
ExprResult Sema::ActOnCXXReflectExpr(SourceLocation Loc,
                                     ParsedReflectionOperand Ref,
                                     SourceLocation LP,
                                     SourceLocation RP) {
  // Translated the parsed reflection operand into an AST operand.
  switch (Ref.getKind()) {
  case ParsedReflectionOperand::Invalid:
    break;
  case ParsedReflectionOperand::Type: {
    QualType Arg = Ref.getAsType().get();
    return BuildCXXReflectExpr(Loc, Arg, LP, RP);
  }
  case ParsedReflectionOperand::Template: {
    TemplateName Arg = Ref.getAsTemplate().get();
    return BuildCXXReflectExpr(Loc, Arg, LP, RP);
  }
  case ParsedReflectionOperand::GlobalNamespace:
  case ParsedReflectionOperand::Namespace: {
    ReflectedNamespace RNS = Ref.getAsNamespace();
    const CXXScopeSpec SS = Ref.getScopeSpec();

    bool IsQualified = !SS.isEmpty();
    if (IsQualified) {
      QualifiedNamespaceName *QNS
        = new (Context) QualifiedNamespaceName(RNS, SS.getScopeRep());
      return BuildCXXReflectExpr(Loc, NamespaceName(QNS), LP, RP);
    }

    return BuildCXXReflectExpr(Loc, NamespaceName(RNS), LP, RP);
  }
  case ParsedReflectionOperand::Expression: {
    Expr *Arg = Ref.getAsExpr();
    return BuildCXXReflectExpr(Loc, Arg, LP, RP);
  }
  }

  return ExprError();
}

ExprResult Sema::BuildCXXReflectExpr(SourceLocation Loc, InvalidReflection *IR,
                                     SourceLocation LP, SourceLocation RP) {
  return CXXReflectExpr::Create(Context, Context.MetaInfoTy, Loc, IR, LP, RP);
}

ExprResult Sema::BuildCXXReflectExpr(SourceLocation Loc, QualType T,
                                     SourceLocation LP, SourceLocation RP) {
  return CXXReflectExpr::Create(Context, Context.MetaInfoTy, Loc, T, LP, RP);
}

ExprResult Sema::BuildCXXReflectExpr(SourceLocation Loc, TemplateName N,
                                     SourceLocation LP, SourceLocation RP) {
  return CXXReflectExpr::Create(Context, Context.MetaInfoTy, Loc, N, LP, RP);
}

ExprResult Sema::BuildCXXReflectExpr(SourceLocation Loc, NamespaceName N,
                                     SourceLocation LP, SourceLocation RP) {
  return CXXReflectExpr::Create(Context, Context.MetaInfoTy, Loc, N, LP, RP);
}

ExprResult Sema::BuildCXXReflectExpr(SourceLocation Loc, Expr *E,
                                     SourceLocation LP, SourceLocation RP) {
  return CXXReflectExpr::Create(Context, Context.MetaInfoTy, Loc, E, LP, RP);
}

ExprResult Sema::BuildCXXReflectExpr(SourceLocation Loc, Decl *D,
                                     SourceLocation LP, SourceLocation RP) {
  return CXXReflectExpr::Create(Context, Context.MetaInfoTy, Loc, D, LP, RP);
}

ExprResult Sema::BuildCXXReflectExpr(SourceLocation Loc, CXXBaseSpecifier *B,
                                     SourceLocation LP, SourceLocation RP) {
  return CXXReflectExpr::Create(Context, Context.MetaInfoTy, Loc, B, LP, RP);
}

ExprResult Sema::BuildInvalidCXXReflectExpr(SourceLocation Loc,
                                         SourceLocation LP, SourceLocation RP) {
  return CXXReflectExpr::CreateInvalid(Context, Context.MetaInfoTy, Loc,
                                       LP, RP);
}

ExprResult Sema::BuildCXXReflectExpr(APValue Reflection, SourceLocation Loc) {
  assert(Reflection.isReflectionVariant());

  switch (Reflection.getReflectionKind()) {
  case RK_invalid: {
    auto ReflOp = const_cast<InvalidReflection *>(
                                         Reflection.getInvalidReflectionInfo());
    return BuildCXXReflectExpr(/*Loc=*/Loc, ReflOp, /*LP=*/SourceLocation(),
                               /*RP=*/SourceLocation());
  }
  case RK_declaration: {
    auto ReflOp = const_cast<Decl *>(Reflection.getReflectedDeclaration());
    return BuildCXXReflectExpr(/*Loc=*/Loc, ReflOp, /*LP=*/SourceLocation(),
                               /*RP=*/SourceLocation());
  }

  case RK_type: {
    auto ReflOp = Reflection.getReflectedType();
    return BuildCXXReflectExpr(/*Loc=*/Loc, ReflOp, /*LP=*/SourceLocation(),
                               /*RP=*/SourceLocation());
  }

  case RK_expression: {
    auto ReflOp = const_cast<Expr *>(Reflection.getReflectedExpression());
    return BuildCXXReflectExpr(/*Loc=*/Loc, ReflOp, /*LP=*/SourceLocation(),
                               /*RP=*/SourceLocation());
  }

  case RK_base_specifier: {
    auto ReflOp = const_cast<CXXBaseSpecifier *>(
                                        Reflection.getReflectedBaseSpecifier());
    return BuildCXXReflectExpr(/*Loc=*/Loc, ReflOp, /*LP=*/SourceLocation(),
                               /*RP=*/SourceLocation());
  }
  case RK_fragment:
    llvm_unreachable("unsupported operation");
  }

  llvm_unreachable("invalid reflection kind");
}

/// Handle a call to \c __invalid_reflection.
ExprResult Sema::ActOnCXXInvalidReflectionExpr(Expr *MessageExpr,
                                               SourceLocation BuiltinLoc,
                                               SourceLocation RParenLoc) {
  if (DiagnoseUnexpandedParameterPack(MessageExpr))
    return ExprError();

  return BuildCXXInvalidReflectionExpr(MessageExpr, BuiltinLoc, RParenLoc);
}

static QualType DeduceCanonicalType(Sema &S, Expr *E) {
  QualType Ty = E->getType();
  if (AutoType *D = Ty->getContainedAutoType()) {
    Ty = D->getDeducedType();
    if (!Ty.getTypePtr())
      llvm_unreachable("Undeduced value reflection");
  }
  return S.Context.getCanonicalType(Ty);
}

/// Build a \c __invalid_reflection expression.
ExprResult Sema::BuildCXXInvalidReflectionExpr(Expr *MessageExpr,
                                               SourceLocation BuiltinLoc,
                                               SourceLocation RParenLoc) {
  assert(MessageExpr != nullptr);

  ExprResult Converted = DefaultFunctionArrayLvalueConversion(MessageExpr);
  if (Converted.isInvalid())
    return ExprError();
  MessageExpr = Converted.get();

  // Get the canonical type of the expression.
  QualType T = DeduceCanonicalType(*this, MessageExpr);

  // Ensure we're working with a valid operand.
  if (!T.isCXXStringLiteralType()) {
    SourceLocation &&Loc = MessageExpr->getExprLoc();
    Diag(Loc, diag::err_invalid_reflection_wrong_operand_type);
    return ExprError();
  }

  return CXXInvalidReflectionExpr::Create(Context, Context.MetaInfoTy,
                                          MessageExpr, BuiltinLoc, RParenLoc);
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

  if (Source != SemaRef.Context.MetaInfoTy) {
    SemaRef.Diag(E->getBeginLoc(), diag::err_expression_not_reflection);
    return false;
  }

  return true;
}

// Validates a query intrinsics' argument count.
static bool CheckArgumentsPresent(Sema &SemaRef,
                                  SourceLocation KWLoc,
                                  const SmallVectorImpl<Expr *> &Args) {
  if (Args.empty()) {
    SemaRef.Diag(KWLoc, diag::err_reflection_query_invalid);
    return true;
  }

  return false;
}

// Sets the query as an unknown query of dependent type. This state
// will be kept, until we either find the correct query and type,
// or fail trying to do so.
static void SetQueryAndTypeUnresolved(Sema &SemaRef, QualType &ResultTy,
                                      ReflectionQuery &Query) {
  ResultTy = SemaRef.Context.DependentTy;
  Query = getUnknownReflectionQuery();
}

static bool CheckQueryType(Sema &SemaRef, Expr *&Arg) {
  QualType T = Arg->getType();

  if (T->isIntegralType(SemaRef.Context)) {
    SemaRef.Diag(Arg->getExprLoc(), diag::err_reflection_query_wrong_type);
    return true;
  }

  return false;
}

static bool SetQuery(Sema &SemaRef, Expr *&Arg, ReflectionQuery &Query) {
  // Evaluate the query operand
  SmallVector<PartialDiagnosticAt, 4> Diags;
  Expr::EvalResult Result;
  Result.Diag = &Diags;
  Expr::EvalContext EvalCtx(SemaRef.Context, SemaRef.GetReflectionCallbackObj());
  if (!Arg->EvaluateAsRValue(Result, EvalCtx)) {
    // FIXME: This is not the right error.
    SemaRef.Diag(Arg->getExprLoc(), diag::err_reflection_query_not_constant);
    for (PartialDiagnosticAt PD : Diags)
      SemaRef.Diag(PD.first, PD.second);
    return true;
  }

  Query = static_cast<ReflectionQuery>(Result.Val.getInt().getExtValue());
  return false;
}

static bool SetType(QualType& Ret, QualType T) {
  Ret = T;
  return false;
}

static QualType GetCStrType(ASTContext &Ctx) {
  return Ctx.getPointerType(Ctx.getConstType(Ctx.CharTy));
}

// Gets the type and query from Arg for a query read operation.
// Returns true on error.
static bool GetTypeAndQueryForRead(Sema &SemaRef, SmallVectorImpl<Expr *> &Args,
                                   QualType &ResultTy,
                                   ReflectionQuery &Query) {
  SetQueryAndTypeUnresolved(SemaRef, ResultTy, Query);

  Expr *QueryArg = Args[0];
  if (QueryArg->isTypeDependent() || QueryArg->isValueDependent())
    return false;

  // Convert to an rvalue.
  ExprResult Conv = SemaRef.DefaultLvalueConversion(QueryArg);
  if (Conv.isInvalid())
    return true;
  Args[0] = QueryArg = Conv.get();

  if (CheckQueryType(SemaRef, QueryArg))
    return true;

  // Resolve the query.
  if (SetQuery(SemaRef, QueryArg, Query))
    return true;

  // Resolve the type.
  if (isPredicateQuery(Query))
    return SetType(ResultTy, SemaRef.Context.BoolTy);
  if (isAssociatedReflectionQuery(Query))
    return SetType(ResultTy, SemaRef.Context.MetaInfoTy);
  if (isNameQuery(Query))
    return SetType(ResultTy, GetCStrType(SemaRef.Context));

  SemaRef.Diag(QueryArg->getExprLoc(), diag::err_reflection_query_invalid);
  return true;
}

// Checks to see if the query arguments match
static bool CheckQueryArgs(Sema &SemaRef, SourceLocation KWLoc,
                           ReflectionQuery Query,
                           SmallVectorImpl<Expr *> &Args) {
  unsigned ExpectedMinArgCount = getMinNumQueryArguments(Query) + 1;
  unsigned ExpectedMaxArgCount = getMaxNumQueryArguments(Query) + 1;
  if (Args.size() < ExpectedMinArgCount || Args.size() > ExpectedMaxArgCount) {
    SemaRef.Diag(KWLoc, diag::err_reflection_wrong_arity)
      << ExpectedMinArgCount << ExpectedMaxArgCount << (int) Args.size();
    return true;
  }

  // FIXME: Check to make sure the arguments are of the correct type.

  return false;
}

ExprResult Sema::ActOnCXXReflectionReadQuery(SourceLocation KWLoc,
                                             SmallVectorImpl<Expr *> &Args,
                                             SourceLocation LParenLoc,
                                             SourceLocation RParenLoc) {
  if (CheckArgumentsPresent(*this, KWLoc, Args))
    return ExprError();

  // Get the type of the query. Note that this will convert Args[0].
  QualType Ty;
  ReflectionQuery Query;
  if (GetTypeAndQueryForRead(*this, Args, Ty, Query))
    return ExprError();

  if (CheckQueryArgs(*this, KWLoc, Query, Args))
    return ExprError();

  // Convert the remaining operands to rvalues.
  for (std::size_t I = 1; I < Args.size(); ++I) {
    ExprResult Arg = DefaultFunctionArrayLvalueConversion(Args[I]);
    if (Arg.isInvalid())
      return ExprError();
    Args[I] = Arg.get();
  }

  return new (Context) CXXReflectionReadQueryExpr(Context, Ty, Query, Args,
                                                  KWLoc, LParenLoc, RParenLoc);
}

// Gets the type and query from Arg for a query write operation.
// Returns false on error.
static bool GetTypeAndQueryForWrite(Sema &SemaRef, SmallVectorImpl<Expr *> &Args,
                                    QualType &ResultTy,
                                    ReflectionQuery &Query) {
  SetQueryAndTypeUnresolved(SemaRef, ResultTy, Query);

  Expr *QueryArg = Args[0];
  if (QueryArg->isTypeDependent() || QueryArg->isValueDependent())
    return false;

  if (CheckQueryType(SemaRef, QueryArg))
    return true;

  // Resolve the query.
  if (SetQuery(SemaRef, QueryArg, Query))
    return true;

  // Resolve the type.
  if (isModifierUpdateQuery(Query))
    return SetType(ResultTy, SemaRef.Context.VoidTy);

  SemaRef.Diag(QueryArg->getExprLoc(), diag::err_reflection_query_invalid);
  return true;
}

ExprResult Sema::ActOnCXXReflectionWriteQuery(SourceLocation KWLoc,
                                              SmallVectorImpl<Expr *> &Args,
                                              SourceLocation LParenLoc,
                                              SourceLocation RParenLoc) {
  if (CheckArgumentsPresent(*this, KWLoc, Args))
    return ExprError();

  // Get the type of the query. Note that this will convert Args[0].
  QualType Ty;
  ReflectionQuery Query;
  if (GetTypeAndQueryForWrite(*this, Args, Ty, Query))
    return ExprError();

  if (CheckQueryArgs(*this, KWLoc, Query, Args))
    return ExprError();

  // Convert the remaining operands to rvalues.
  for (std::size_t I = 0; I < Args.size(); ++I) {
    if (I == 1)
      continue;

    ExprResult Arg = DefaultFunctionArrayLvalueConversion(Args[I]);
    if (Arg.isInvalid())
      return ExprError();
    Args[I] = Arg.get();
  }

  return new (Context) CXXReflectionWriteQueryExpr(Context, Ty, Query, Args,
                                                   KWLoc, LParenLoc, RParenLoc);
}

template<template<typename> class Collection>
static bool HasDependentParts(Collection<Expr *>& Parts) {
 return std::any_of(Parts.begin(), Parts.end(), [](const Expr *E) {
   return E->isTypeDependent() || E->isValueDependent();
 });
}

ExprResult Sema::ActOnCXXReflectPrintLiteral(SourceLocation KWLoc,
                                             SmallVectorImpl<Expr *> &Args,
                                             SourceLocation LParenLoc,
                                             SourceLocation RParenLoc) {
  if (HasDependentParts(Args))
    return new (Context) CXXReflectPrintLiteralExpr(
        Context, Context.DependentTy, Args, KWLoc, LParenLoc, RParenLoc);

  for (std::size_t I = 0; I < Args.size(); ++I) {
    Expr *&E = Args[I];

    assert(!E->isTypeDependent() && !E->isValueDependent()
        && "Dependent element");

    /// Convert to rvalue.
    ExprResult Conv = DefaultFunctionArrayLvalueConversion(E);
    if (Conv.isInvalid())
      return ExprError();
    E = Conv.get();

    // Get the canonical type of the expression.
    QualType T = DeduceCanonicalType(*this, E);

    // Ensure we're working with a valid operand.
    if (T.isCXXStringLiteralType())
      continue;

    if (T->isIntegerType())
      continue;

    Diag(E->getExprLoc(), diag::err_reflect_print_literal_wrong_operand_type);
    return ExprError();
  }

  return new (Context) CXXReflectPrintLiteralExpr(
    Context, Context.IntTy, Args, KWLoc, LParenLoc, RParenLoc);
}

ExprResult Sema::ActOnCXXReflectPrintReflection(SourceLocation KWLoc,
                                                Expr *Reflection,
                                                SourceLocation LParenLoc,
                                                SourceLocation RParenLoc) {
  if (Reflection->isTypeDependent() || Reflection->isValueDependent())
    return new (Context) CXXReflectPrintReflectionExpr(
        Context, Context.DependentTy, Reflection, KWLoc, LParenLoc, RParenLoc);

  ExprResult Arg = DefaultLvalueConversion(Reflection);
  if (Arg.isInvalid())
    return ExprError();

  if (!CheckReflectionOperand(*this, Reflection))
    return ExprError();

  return new (Context) CXXReflectPrintReflectionExpr(
      Context, Context.IntTy, Arg.get(), KWLoc, LParenLoc, RParenLoc);
}

ExprResult Sema::ActOnCXXReflectDumpReflection(SourceLocation KWLoc,
                                               Expr *Reflection,
                                               SourceLocation LParenLoc,
                                               SourceLocation RParenLoc) {
  if (Reflection->isTypeDependent() || Reflection->isValueDependent())
    return new (Context) CXXReflectDumpReflectionExpr(
        Context, Context.DependentTy, Reflection, KWLoc, LParenLoc, RParenLoc);

  ExprResult Arg = DefaultLvalueConversion(Reflection);
  if (Arg.isInvalid())
    return ExprError();

  if (!CheckReflectionOperand(*this, Reflection))
    return ExprError();

  return new (Context) CXXReflectDumpReflectionExpr(
      Context, Context.IntTy, Arg.get(), KWLoc, LParenLoc, RParenLoc);
}

/// Handle a call to \c __compiler_error.
ExprResult Sema::ActOnCXXCompilerErrorExpr(Expr *MessageExpr,
                                           SourceLocation BuiltinLoc,
                                           SourceLocation RParenLoc) {
  if (DiagnoseUnexpandedParameterPack(MessageExpr))
    return ExprError();

  return BuildCXXCompilerErrorExpr(MessageExpr, BuiltinLoc, RParenLoc);
}

/// Build a \c __compiler_error expression.
ExprResult Sema::BuildCXXCompilerErrorExpr(Expr *MessageExpr,
                                           SourceLocation BuiltinLoc,
                                           SourceLocation RParenLoc) {
  assert(MessageExpr != nullptr);

  ExprResult Converted = DefaultFunctionArrayLvalueConversion(MessageExpr);
  if (Converted.isInvalid())
    return ExprError();
  MessageExpr = Converted.get();

  // Get the canonical type of the expression.
  QualType T = DeduceCanonicalType(*this, MessageExpr);

  // Ensure we're working with a valid operand.
  if (!T.isCXXStringLiteralType()) {
    SourceLocation &&Loc = MessageExpr->getExprLoc();
    Diag(Loc, diag::err_compiler_error_wrong_operand_type);
    return ExprError();
  }

  return CXXCompilerErrorExpr::Create(Context, Context.VoidTy, MessageExpr,
                                      BuiltinLoc, RParenLoc);
}

static DeclRefExpr *DeclReflectionToDeclRefExpr(
    Sema &S, const Reflection &R, SourceLocation SL) {
  assert(R.isDeclaration());

  // If this is a value declaration, then construct a DeclRefExpr.
  if (const ValueDecl *VD = dyn_cast<ValueDecl>(R.getAsDeclaration())) {
    QualType T = VD->getType();
    ValueDecl *NCVD = const_cast<ValueDecl *>(VD);
    ExprValueKind VK = S.getValueKindForDeclReference(T, NCVD, SL);
    return S.BuildDeclRefExpr(NCVD, T, VK, SL);
  }

  return nullptr;
}

static DeclRefExpr *UseDeclRefExprForIdExpr(Sema &S, const DeclRefExpr *DRE) {
  Decl *ReferencedDecl = const_cast<NamedDecl *>(DRE->getFoundDecl());
  ReferencedDecl->markUsed(S.Context);
  return const_cast<DeclRefExpr *>(DRE);
}

static UnresolvedLookupExpr *UseUnresolvedLookupExprForIdExpr(
    Sema &S, const UnresolvedLookupExpr *ULE) {
  return const_cast<UnresolvedLookupExpr *>(ULE);
}

static ExprResult getIdExprReflectedExpr(Sema &SemaRef, Expr *Refl) {
  SourceLocation ReflLoc = Refl->getExprLoc();

  Reflection R;
  if (EvaluateReflection(SemaRef, Refl, R))
    return ExprError();

  if (R.isInvalid()) {
    DiagnoseInvalidReflection(SemaRef, Refl, R);
    return ExprError();
  }

  if (R.isDeclaration()) {
    if (const DeclRefExpr *E = DeclReflectionToDeclRefExpr(SemaRef, R, ReflLoc))
      return UseDeclRefExprForIdExpr(SemaRef, E);
  }

  if (R.isExpression()) {
    if (const auto *DRE = dyn_cast<DeclRefExpr>(R.getAsExpression()))
      return UseDeclRefExprForIdExpr(SemaRef, DRE);

    if (const auto *ULE = dyn_cast<UnresolvedLookupExpr>(R.getAsExpression()))
      return UseUnresolvedLookupExprForIdExpr(SemaRef, ULE);
  }

  // FIXME: Emit a better error diagnostic.
  SemaRef.Diag(ReflLoc, diag::err_expression_not_value_reflection);
  return ExprError();
}

ExprResult Sema::ActOnCXXIdExprExpr(SourceLocation KWLoc,
                                    Expr *Refl,
                                    SourceLocation LParenLoc,
                                    SourceLocation RParenLoc,
                                    SourceLocation EllipsisLoc) {
  if (Refl->isTypeDependent() || Refl->isValueDependent())
    return CXXIdExprExpr::Create(Context, Refl, KWLoc, LParenLoc, LParenLoc);

  return getIdExprReflectedExpr(*this, Refl);
}

ExprResult Sema::ActOnCXXMemberIdExprExpr(
    Expr *Base, Expr *Refl, bool IsArrow,
    SourceLocation OpLoc, SourceLocation TemplateKWLoc,
    SourceLocation LParenLoc, SourceLocation RParenLoc,
    const TemplateArgumentListInfo *TemplateArgs) {
  if (Refl->isTypeDependent() || Refl->isValueDependent())
    return CXXMemberIdExprExpr::Create(
        Context, Base, Refl, IsArrow, OpLoc, TemplateKWLoc,
        LParenLoc, LParenLoc, TemplateArgs);

  // Get the reflection evaluated then reexpressed as a DeclRefExpr
  // or an UnresolvedLookupExpr.
  //
  // FIXME: We can refactor this to avoid the creation of this intermediary
  // expression
  ExprResult ReflectedExprResult = getIdExprReflectedExpr(*this, Refl);
  if (ReflectedExprResult.isInvalid())
    return ExprError();

  // Extract the relevant information for transform into an
  // UnresolvedMemberExpr.
  Expr *ReflectedExpr = ReflectedExprResult.get();

  DeclarationNameInfo NameInfo;
  NestedNameSpecifierLoc QualifierLoc;
  UnresolvedSet<8> ToDecls;

  if (auto *DRE = dyn_cast<DeclRefExpr>(ReflectedExpr)) {
    NamedDecl *Named = DRE->getDecl();

    if (auto *FD = dyn_cast<FieldDecl>(Named)) {
      return BuildFieldReferenceExpr(
          Base, IsArrow, OpLoc, /*SS*/{}, FD,
          DeclAccessPair::make(FD, FD->getAccess()),
          Named->getNameInfo());
    } else {
      NameInfo = Named->getNameInfo();
      ToDecls.addDecl(Named);
    }
  } else if (auto *ULE = dyn_cast<UnresolvedLookupExpr>(ReflectedExpr)) {
    NameInfo = ULE->getNameInfo();
    QualifierLoc = ULE->getQualifierLoc();
    for (auto *D : ULE->decls()) {
      ToDecls.addDecl(D);
    }
  } else {
    ReflectedExpr->dump();
    llvm_unreachable("reflection resolved to unsupported expression");
  }

  // Verify we have valid data
  for (Decl *D : ToDecls) {
    if (isa<FunctionTemplateDecl>(D))
      D = cast<FunctionTemplateDecl>(D)->getTemplatedDecl();

    if (isa<CXXMethodDecl>(D) || isa<FieldDecl>(D))
      continue;

    Diag(ReflectedExpr->getExprLoc(), diag::err_reflection_not_member);
    return ExprError();
  }

  return UnresolvedMemberExpr::Create(
      Context, /*HasUnresolvedUsing=*/false, Base, Base->getType(), IsArrow,
      OpLoc, QualifierLoc, TemplateKWLoc, NameInfo,
      TemplateArgs, ToDecls.begin(), ToDecls.end());
}

ExprResult Sema::ActOnCXXMemberIdExprExpr(
    Expr *Base, Expr *Refl, bool IsArrow,
    SourceLocation OpLoc, SourceLocation TemplateKWLoc,
    SourceLocation LParenLoc, SourceLocation RParenLoc,
    SourceLocation LAngleLoc, ASTTemplateArgsPtr TemplateArgsPtr,
    SourceLocation RAngleLoc) {
  TemplateArgumentListInfo TemplateArgs(LAngleLoc, RAngleLoc);
  translateTemplateArguments(TemplateArgsPtr, TemplateArgs);

  return ActOnCXXMemberIdExprExpr(
      Base, Refl, IsArrow, OpLoc, TemplateKWLoc,
      LParenLoc, RParenLoc, &TemplateArgs);
}

static Expr *ExprReflectionToValueExpr(Sema &S, const Reflection &R,
                                       SourceLocation SL) {
  assert(R.isExpression());

  return const_cast<Expr *>(R.getAsExpression());
}

static Expr *BuildInitialReflectionToValueExpr(Sema &S, const Reflection &R,
                                               SourceLocation SL) {
  switch (R.getKind()) {
  case RK_declaration:
    return DeclReflectionToDeclRefExpr(S, R, SL);
  case RK_expression:
    return ExprReflectionToValueExpr(S, R, SL);
  default:
    return nullptr;
  }
}

static Expr *DeclRefExprToValueExpr(Sema &S, DeclRefExpr *Ref) {
  // If the expression we're going to evaluate is a reference to a field.
  // Adjust this to be a pointer to that field.
  if (const FieldDecl *F = dyn_cast<FieldDecl>(Ref->getDecl())) {
    QualType Ty = F->getType();
    const Type *Cls = S.Context.getTagDeclType(F->getParent()).getTypePtr();
    Ty = S.Context.getMemberPointerType(Ty, Cls);
    return UnaryOperator::Create(
        S.Context, Ref, UO_AddrOf, Ty, VK_RValue, OK_Ordinary,
        Ref->getExprLoc(), /*CanOverflow=*/false, S.CurFPFeatureOverrides());
  }

  return Ref;
}

static Expr *CompleteReflectionToValueExpr(Sema &S, Expr *EvalExpr) {
  if (DeclRefExpr *Ref = dyn_cast_or_null<DeclRefExpr>(EvalExpr))
    return DeclRefExprToValueExpr(S, Ref);

  return EvalExpr;
}

static Expr *ReflectionToValueExpr(Sema &S, const Reflection &R,
                                   SourceLocation SL) {
  // Handle the initial transformation from reflection
  // to expression.
  Expr *EvalExpr = BuildInitialReflectionToValueExpr(S, R, SL);

  // Modify the initial expression to be properly
  // evaluable during constexpr eval.
  EvalExpr = CompleteReflectionToValueExpr(S, EvalExpr);

  // Ensure the expression results in an rvalue.
  ExprResult Res = S.DefaultFunctionArrayLvalueConversion(EvalExpr);
  if (Res.isInvalid())
    return nullptr;

  return Res.get();
}

bool
Sema::ExpansionContextBuilder::BuildCalls()
{
  SourceLocation Loc;

  // If the range is dependent, mark it and return. We'll transform it later.
  if (Range->getType()->isDependentType() || Range->isValueDependent()) {
    Kind = RK_Unknown;
    return false;
  }

  if (Range->getType()->isConstantArrayType()) {
    Kind = RK_Array;
    return BuildArrayCalls();
  }

  Kind = RK_Range;

  // Get the std namespace for std::begin and std::end
  NamespaceDecl *Std = SemaRef.getOrCreateStdNamespace();

  // Get the info for a call to std::begin
  DeclarationNameInfo BeginNameInfo(
      &SemaRef.Context.Idents.get("begin"), Loc);
  LookupResult BeginCallLookup(SemaRef, BeginNameInfo, Sema::LookupOrdinaryName);
  if (!SemaRef.LookupQualifiedName(BeginCallLookup, Std))
    return true;
  if (BeginCallLookup.getResultKind() != LookupResult::FoundOverloaded)
    return true;
  UnresolvedLookupExpr *BeginFn =
    UnresolvedLookupExpr::Create(SemaRef.Context, /*NamingClass=*/nullptr,
                                 NestedNameSpecifierLoc(), BeginNameInfo,
                                 /*ADL=*/true, /*Overloaded=*/true,
                                 BeginCallLookup.begin(),
                                 BeginCallLookup.end());

  // Get the info for a call to std::end
  DeclarationNameInfo EndNameInfo(
      &SemaRef.Context.Idents.get("end"), Loc);
  LookupResult EndCallLookup(SemaRef, EndNameInfo, Sema::LookupOrdinaryName);
  if (!SemaRef.LookupQualifiedName(EndCallLookup, Std))
    return true;
  if (EndCallLookup.getResultKind() != LookupResult::FoundOverloaded)
    return true;
  UnresolvedLookupExpr *EndFn =
    UnresolvedLookupExpr::Create(SemaRef.Context, /*NamingClass=*/nullptr,
                                 NestedNameSpecifierLoc(), EndNameInfo,
                                 /*ADL=*/true, /*Overloaded=*/true,
                                 EndCallLookup.begin(),
                                 EndCallLookup.end());

  // Build the actual calls
  Expr *Args[] = {Range};
  ExprResult BeginCall =
    SemaRef.ActOnCallExpr(CurScope, BeginFn, Loc, Args, Loc);
  if (BeginCall.isInvalid())
    return true;
  ExprResult EndCall =
    SemaRef.ActOnCallExpr(CurScope, EndFn, Loc, Args, Loc);
  if (EndCall.isInvalid())
    return true;

  RangeBegin = BeginCall.get();
  RangeEnd = EndCall.get();

  return false;
}

bool
Sema::ExpansionContextBuilder::BuildArrayCalls()
{
  // For an array arr, RangeBegin is arr[0]
  IntegerLiteral *ZeroIndex =
    IntegerLiteral::Create(SemaRef.Context, llvm::APSInt::getUnsigned(0),
                           SemaRef.Context.getSizeType(), SourceLocation());

  ExprResult BeginAccessor =
    SemaRef.ActOnArraySubscriptExpr(CurScope, Range, SourceLocation(),
                                    ZeroIndex, SourceLocation());
  if (BeginAccessor.isInvalid())
    return true;

  RangeBegin = BeginAccessor.get();

  // For an array of size N, RangeEnd is N (not arr[N])
  // Get the canonical type here, as some transformations may
  // have been applied earlier on.
  ConstantArrayType const *ArrayTy = cast<ConstantArrayType>(
    Range->getType()->getCanonicalTypeInternal());
  llvm::APSInt Last(ArrayTy->getSize(), true);
  IntegerLiteral *LastIndex =
    IntegerLiteral::Create(SemaRef.Context, Last,
                           SemaRef.Context.getSizeType(), SourceLocation());
  RangeEnd = LastIndex;
  return false;
}

static ExprResult
BuildSubscriptAccess(Sema &SemaRef, Expr *Current, std::size_t Index)
{
  ASTContext &Context = SemaRef.Context;

  // The subscript.
  IntegerLiteral *E =
    IntegerLiteral::Create(Context, llvm::APSInt::getUnsigned(Index),
                           Context.getSizeType(), SourceLocation());

  // Get the actual array base to create an incremented subscript access.
  ArraySubscriptExpr *CurrentSubscript =
    static_cast<ArraySubscriptExpr*>(Current);
  Expr *Base = CurrentSubscript->getBase();

  ExprResult RangeAccessor =
    SemaRef.ActOnArraySubscriptExpr(SemaRef.getCurScope(), Base,
                                    SourceLocation(), E, SourceLocation());
  if (RangeAccessor.isInvalid())
    return ExprError();
  return RangeAccessor;
}

// Build a call to std::next(Arg, Induction) for the range traverser
static ExprResult
BuildNextCall(Sema &SemaRef, Expr *Arg, Expr *Induction)
{
  NamespaceDecl *Std = SemaRef.getOrCreateStdNamespace();
  SourceLocation Loc = SourceLocation();

  DeclarationNameInfo NextNameInfo(
    &SemaRef.Context.Idents.get("next"), Loc);
  LookupResult NextCallLookup(SemaRef, NextNameInfo, Sema::LookupOrdinaryName);
  if (!SemaRef.LookupQualifiedName(NextCallLookup, Std))
    return ExprError();
  if (NextCallLookup.getResultKind() != LookupResult::FoundOverloaded)
    return ExprError();

  UnresolvedLookupExpr *NextFn =
    UnresolvedLookupExpr::Create(SemaRef.Context, /*NamingClass=*/nullptr,
                                 NestedNameSpecifierLoc(), NextNameInfo,
                                 /*ADL=*/true, /*Overloaded=*/true,
                                 NextCallLookup.begin(),
                                 NextCallLookup.end());
  Expr *Args[] = {Arg, Induction};
  ExprResult NextCall =
    SemaRef.ActOnCallExpr(SemaRef.getCurScope(), NextFn, Loc, Args, Loc);
  if (NextCall.isInvalid())
    return ExprError();

  return NextCall;
}

// Dereference a call to an iterator (as built in BuildNextCall)
static ExprResult
BuildDeref(Sema &SemaRef, Expr *NextCall)
{
  ExprResult NextDeref =
    SemaRef.ActOnUnaryOp(SemaRef.getCurScope(), SourceLocation(),
                         tok::star, NextCall);
  if (NextDeref.isInvalid())
    return ExprError();
  return NextDeref;
}

// Checks to see if the traversal is complete.
//
// Returns true when traversal is complete.
Sema::RangeTraverser::operator bool()
{
  switch (Kind) {
  case RK_Array:
    // If this is an array, we will simply see if we have iterated
    // N times.
    return I == cast<IntegerLiteral>(RangeEnd)->getValue();

  case RK_Range: {
    // Check to see if we've hit the end of the range.
    SmallVector<PartialDiagnosticAt, 4> Diags;
    Expr::EvalResult EqualRes;
    EqualRes.Diag = &Diags;

    ExprResult Equal =
        SemaRef.ActOnBinOp(SemaRef.getCurScope(), SourceLocation(),
                           tok::equalequal, Current, RangeEnd);

    Expr *EqualExpr = Equal.get();

    Expr::EvalContext EvalCtx(SemaRef.Context,
                              SemaRef.GetReflectionCallbackObj());
    if (!EqualExpr->EvaluateAsRValue(EqualRes, EvalCtx)) {
      SemaRef.Diag(SourceLocation(),
                   diag::err_constexpr_range_iteration_failed);
      for (PartialDiagnosticAt PD : Diags)
        SemaRef.Diag(PD.first, PD.second);
      return true;
    }

    return EqualRes.Val.getInt() == 1;
  }
  default:
    llvm_unreachable("Invalid Range Kind.");
  }
}

Expr *
Sema::RangeTraverser::operator*()
{
  Expr *Deref = nullptr;
  switch (Kind) {
  case RK_Range:
    Deref = BuildDeref(SemaRef, Current).get();
    break;
  case RK_Array:
    Deref = Current;
    break;
  default:
    break;
  }

  ExprResult RvalueDeref = SemaRef.DefaultLvalueConversion(Deref);

  assert(!RvalueDeref.isInvalid() && "Could not dereference range member.");
  return RvalueDeref.get();
}

Sema::RangeTraverser &
Sema::RangeTraverser::operator++()
{
  IntegerLiteral *Index =
  IntegerLiteral::Create(SemaRef.Context, llvm::APSInt::getUnsigned(1),
                         SemaRef.Context.getSizeType(), SourceLocation());
  const auto done = operator bool();
  if (!done) {
    ++I;
    ExprResult Next;
    switch (Kind) {
    case RK_Range:
      Next = BuildNextCall(SemaRef, Current, Index);
      break;
    case RK_Array:
      Next = BuildSubscriptAccess(SemaRef, Current, I);
      break;
    default:
      break;
    }

    if (Next.isInvalid())
      llvm_unreachable("Could not build next call.");
    Current = Next.get();
  }

  return *this;
}

static ExprResult
getAsCXXValueOfExpr(Sema &SemaRef, Expr *Expression,
                    SourceLocation EllipsisLoc = SourceLocation())
{
  // llvm::outs() << "The expression:\n";
  // Expression->dump();
  return SemaRef.ActOnCXXValueOfExpr (SourceLocation(), Expression,
                                      SourceLocation(), SourceLocation(),
                                      EllipsisLoc);
}

static ExprResult
getAsCXXIdExprExpr(Sema &SemaRef, Expr *Expression,
                   SourceLocation EllipsisLoc = SourceLocation())
{
  return SemaRef.ActOnCXXIdExprExpr(SourceLocation(), Expression,
                                    SourceLocation(), SourceLocation(),
                                    EllipsisLoc);
}

static ExprResult
getAsCXXIdentifierSplice(Sema &SemaRef, Expr *Expression)
{
  SourceLocation &&Loc = Expression->getExprLoc();

  ArrayRef<Expr *> Parts(&Expression, 1);

  IdentifierInfo *II;
  if (SemaRef.ActOnCXXIdentifierSplice(Parts, II))
    return ExprError();

  CXXScopeSpec NewSS;
  DeclarationNameInfo DNI(II, Loc);

  ParserLookupSetup ParserLookup(SemaRef, SemaRef.CurContext);
  ExprResult BuiltExpr =
    SemaRef.ActOnIdExpression(ParserLookup.getCurScope(), NewSS,
                              SourceLocation(), DNI,
                              /*TemplateArgsPtr=*/nullptr,
                              UnqualifiedIdKind::IK_Identifier,
                              /*TemplateID=*/nullptr,
                              /*HasTrailingLParen=*/false,
                              /*IsAddresOfOperand=*/false,
                              /*CCC=*/nullptr,
                              /*IsInlineAsmIdentifier=*/false,
                              /*KeywordReplacement=*/nullptr);

  return BuiltExpr.isInvalid() ? ExprError() : BuiltExpr;
}

static QualType
getAsCXXReflectedType(Sema &SemaRef, Expr *Expression)
{
  return SemaRef.BuildReflectedType(SourceLocation(), Expression);
}

bool Sema::ActOnVariadicReifier(
    llvm::SmallVectorImpl<Expr *> &Expressions, SourceLocation KWLoc,
    IdentifierInfo *KW, Expr *Range, SourceLocation LParenLoc,
    SourceLocation EllipsisLoc, SourceLocation RParenLoc) {
  Sema::ExpansionContextBuilder CtxBldr(*this, getCurScope(), Range);
  if (CtxBldr.BuildCalls()) {
    // TODO: Diag << failed to build calls
    return true;
  }

  ExprResult C;

  // If the expansion is dependent, we cannot expand on it yet.
  // Just return an empty reifier containing the Range and EllipsisLoc.
  if (CtxBldr.getKind() == RK_Unknown) {
    C = ActOnCXXDependentVariadicReifierExpr(Range, KWLoc, KW, LParenLoc,
                                             EllipsisLoc, RParenLoc);
    Expressions.push_back(C.get());
    return false;
  }
  // Traverse the range now and add the exprs to the vector
  Sema::RangeTraverser Traverser(*this,
                                 CtxBldr.getKind(),
                                 CtxBldr.getRangeBeginCall(),
                                 CtxBldr.getRangeEndCall());

  while (!Traverser) {
    switch (KW->getTokenID()) {
    case tok::kw_valueof:
      C = getAsCXXValueOfExpr(*this, *Traverser);
      break;
    case tok::kw_idexpr:
      C = getAsCXXIdExprExpr(*this, *Traverser);
      break;
    case tok::kw_unqualid:
      C = getAsCXXIdentifierSplice(*this, *Traverser);
      break;
    case tok::kw_typename:
      Diag(KWLoc, diag::err_invalid_reifier_context) << 3 << 0;
      return true;
    case tok::kw_namespace:
      Diag(KWLoc, diag::err_namespace_as_variadic_reifier);
      return true;
    default: // silence warning
      break;
    }

    if (!C.isInvalid() && C.isUsable())
      Expressions.push_back(C.get());
    else
      return true;

    ++Traverser;
  }

  return false;
}

bool Sema::ActOnVariadicReifier(
    llvm::SmallVectorImpl<QualType> &Types, SourceLocation KWLoc,
    Expr *Range, SourceLocation LParenLoc, SourceLocation EllipsisLoc,
    SourceLocation RParenLoc) {
  Sema::ExpansionContextBuilder CtxBldr(*this, getCurScope(), Range);
  if (CtxBldr.BuildCalls()) {
    // TODO: Diag << failed to build calls
    return true;
  }

  if (CtxBldr.getKind() == RK_Unknown) {
    QualType T =
      Context.getCXXDependentVariadicReifierType(Range, KWLoc,
                                                 RParenLoc, EllipsisLoc);
    Types.push_back(T);
    return false;
  }

  // Traverse the range now and add the exprs to the vector
  RangeTraverser Traverser(*this,
                           CtxBldr.getKind(),
                           CtxBldr.getRangeBeginCall(),
                           CtxBldr.getRangeEndCall());

  while(!Traverser) {
    QualType T = getAsCXXReflectedType(*this, *Traverser);

    if (T.isNull())
      return true;

    Types.push_back(T);

    ++Traverser;
  }

  return false;
}

ExprResult Sema::ActOnCXXValueOfExpr(SourceLocation KWLoc,
                                     Expr *Refl,
                                     SourceLocation LParenLoc,
                                     SourceLocation RParenLoc,
                                     SourceLocation EllipsisLoc)
{
  if (Refl->isTypeDependent() || Refl->isValueDependent())
    return new (Context) CXXValueOfExpr(Context.DependentTy, Refl, KWLoc,
                                        LParenLoc, LParenLoc, EllipsisLoc);

  if (!CheckReflectionOperand(*this, Refl))
    return ExprError();

  Reflection R;
  if (EvaluateReflection(*this, Refl, R))
    return ExprError();

  if (R.isInvalid()) {
    DiagnoseInvalidReflection(*this, Refl, R);
    return ExprError();
  }

  Expr *Eval = ReflectionToValueExpr(*this, R, KWLoc);
  if (!Eval) {
    Diag(Refl->getExprLoc(), diag::err_expression_not_value_reflection);
    return ExprError();
  }

  // Evaluate the resulting expression.
  SmallVector<PartialDiagnosticAt, 4> Diags;
  Expr::EvalResult Result;
  Result.Diag = &Diags;
  Expr::EvalContext EvalCtx(Context, GetReflectionCallbackObj());
  if (!Eval->EvaluateAsRValue(Result, EvalCtx)) {
    Diag(Eval->getExprLoc(), diag::err_reflection_reflects_non_constant_expression);
    for (PartialDiagnosticAt PD : Diags)
      Diag(PD.first, PD.second);
    return ExprError();
  }

  return ConstantExpr::Create(Context, Eval, std::move(Result.Val));
}

ExprResult Sema::ActOnCXXDependentVariadicReifierExpr(Expr *Range,
                                                      SourceLocation KWLoc,
                                                      IdentifierInfo *KW,
                                                      SourceLocation LParenLoc,
                                                      SourceLocation EllipsisLoc,
                                                      SourceLocation RParenLoc)
{
  // If the dependent reifier isn't dependent, something has gone wrong.
  assert((Range->isTypeDependent() || Range->isValueDependent()) &&
         "Marked a non-dependent variadic reifier as dependent.");

  return new (Context) CXXDependentVariadicReifierExpr(Context.DependentTy,
                                                       Range, KWLoc, KW,
                                                       LParenLoc, LParenLoc,
                                                       EllipsisLoc);
}


static bool AppendStringValue(Sema& S, llvm::raw_ostream& OS,
                              const APValue& Val) {
  // Extracting the string value from the LValue.
  //
  // FIXME: We probably want something like EvaluateAsString in the Expr class.
  APValue::LValueBase Base = Val.getLValueBase();
  if (Base.is<const Expr *>()) {
    const Expr *BaseExpr = Base.get<const Expr *>();
    assert(isa<StringLiteral>(BaseExpr) && "Not a string literal");
    const StringLiteral *Str = cast<StringLiteral>(BaseExpr);
    OS << Str->getString();
  } else {
    llvm_unreachable("Use of string variable not implemented");
    // const ValueDecl *D = Base.get<const ValueDecl *>();
    // return Error(E->getMessage());
  }
  return true;
}

static bool AppendCharacterArray(Sema& S, llvm::raw_ostream &OS, Expr *E,
                                 QualType T) {
  assert(T->isArrayType() && "Not an array type");
  const ArrayType *ArrayTy = cast<ArrayType>(T.getTypePtr());

  // Check that the type is 'const char[N]' or 'char[N]'.
  QualType ElemTy = ArrayTy->getElementType();
  if (!ElemTy->isCharType()) {
    S.Diag(E->getBeginLoc(), diag::err_reflected_id_invalid_operand_type) << T;
    return false;
  }

  // Evaluate the expression.
  Expr::EvalResult Result;
  Expr::EvalContext EvalCtx(S.Context, S.GetReflectionCallbackObj());
  if (!E->EvaluateAsLValue(Result, EvalCtx)) {
    // FIXME: Include notes in the diagnostics.
    S.Diag(E->getBeginLoc(), diag::err_expr_not_ice) << 1;
    return false;
  }

  return AppendStringValue(S, OS, Result.Val);
}

static bool AppendCharacterPointer(Sema& S, llvm::raw_ostream &OS, Expr *E,
                                   QualType T) {
  assert(T->isPointerType() && "Not a pointer type");
  const PointerType* PtrTy = cast<PointerType>(T.getTypePtr());

  // Check for 'const char*'.
  QualType ElemTy = PtrTy->getPointeeType();
  if (!ElemTy->isCharType() || !ElemTy.isConstQualified()) {
    S.Diag(E->getBeginLoc(), diag::err_reflected_id_invalid_operand_type) << T;
    return false;
  }

  // Try evaluating the expression as an rvalue and then extract the result.
  Expr::EvalResult Result;
  Expr::EvalContext EvalCtx(S.Context, S.GetReflectionCallbackObj());
  if (!E->EvaluateAsRValue(Result, EvalCtx)) {
    // FIXME: This is not the right error.
    S.Diag(E->getBeginLoc(), diag::err_expr_not_ice) << 1;
    return false;
  }

  return AppendStringValue(S, OS, Result.Val);
}

static bool AppendInteger(Sema& S, llvm::raw_ostream &OS, Expr *E, QualType T) {
  Expr::EvalResult Result;
  Expr::EvalContext EvalCtx(S.Context, S.GetReflectionCallbackObj());
  if (!E->EvaluateAsInt(Result, EvalCtx)) {
    S.Diag(E->getBeginLoc(), diag::err_expr_not_ice) << 1;
    return false;
  }
  OS << Result.Val.getInt();
  return true;
}

static inline bool
AppendReflectedDecl(Sema &S, llvm::raw_ostream &OS, const Expr *ReflExpr,
                    const Decl *D) {
  // If this is a named declaration, append its identifier.
  if (!isa<NamedDecl>(D)) {
    // FIXME: Improve diagnostics.
    S.Diag(ReflExpr->getBeginLoc(), diag::err_reflection_not_named);
    return false;
  }
  const NamedDecl *ND = cast<NamedDecl>(D);

  // FIXME: What if D has a special name? For example operator==?
  // What would we append in that case?
  DeclarationName Name = ND->getDeclName();
  if (!Name.isIdentifier()) {
    S.Diag(ReflExpr->getBeginLoc(), diag::err_reflected_id_not_an_identifer) << Name;
    return false;
  }

  OS << ND->getName();
  return true;
}

static inline bool
AppendReflectedType(Sema& S, llvm::raw_ostream &OS, const Expr *ReflExpr,
                    const QualType &QT) {
  const Type *T = QT.getTypePtr();

  // If this is a class type, append its identifier.
  if (auto *RC = T->getAsCXXRecordDecl())
    OS << RC->getName();
  else if (const BuiltinType *BT = T->getAs<BuiltinType>())
    OS << BT->getName();
  else {
    S.Diag(ReflExpr->getBeginLoc(), diag::err_reflected_id_not_an_identifer)
      << QualType(T, 0);
    return false;
  }
  return true;
}

static bool
AppendReflection(Sema& S, llvm::raw_ostream &OS, Expr *E) {
  Reflection Refl;
  if (EvaluateReflection(S, E, Refl))
    return false;

  switch (Refl.getKind()) {
  case RK_invalid:
    DiagnoseInvalidReflection(S, E, Refl);
    return false;

  case RK_type: {
    const QualType QT = Refl.getAsType();
    return AppendReflectedType(S, OS, E, QT);
  }

  case RK_declaration: {
    const Decl *D = Refl.getAsDeclaration();
    return AppendReflectedDecl(S, OS, E, D);
  }

  case RK_expression: {
    const Expr *RE = Refl.getAsExpression();
    if (const DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(RE))
      return AppendReflectedDecl(S, OS, E, DRE->getDecl());
    break;
  }

  case RK_base_specifier:
  case RK_fragment:
    break;
  }

  llvm_unreachable("Unsupported reflection type");
}

bool Sema::ActOnCXXIdentifierSplice(
    ArrayRef<Expr *> Parts, IdentifierInfo *&Result) {
  return BuildCXXIdentifierSplice(Parts, Result);
}

/// Constructs a new identifier from the expressions in Parts.
///
/// Returns true upon error.
bool Sema::BuildCXXIdentifierSplice(
    ArrayRef<Expr *> Parts, IdentifierInfo *&Result) {
  // If any components are dependent, we can't compute the name.
  if (HasDependentParts(Parts)) {
    Result = &Context.Idents.get(Context, Parts);
    return false;
  }

  SmallString<256> Buf;
  llvm::raw_svector_ostream OS(Buf);
  for (std::size_t I = 0; I < Parts.size(); ++I) {
    Expr *E = Parts[I];

    assert(!E->isTypeDependent() && !E->isValueDependent()
        && "Dependent name component");

    // Get the type of the reflection.
    QualType T = DeduceCanonicalType(*this, E);

    SourceLocation ExprLoc = E->getBeginLoc();

    // Evaluate the sub-expression (depending on type) in order to compute
    // a string part that will constitute a declaration name.
    if (T->isConstantArrayType()) {
      if (!AppendCharacterArray(*this, OS, E, T))
        return true;
    }
    else if (T->isPointerType()) {
      if (!AppendCharacterPointer(*this, OS, E, T))
        return true;
    }
    else if (T->isIntegerType()) {
      if (I == 0) {
        // An identifier cannot start with an integer value.
        Diag(ExprLoc, diag::err_reflected_id_with_integer_prefix);
        return true;
      }
      if (!AppendInteger(*this, OS, E, T))
        return true;
    }
    else if (CheckReflectionOperand(*this, E)) {
      if (!AppendReflection(*this, OS, E))
        return true;
    }
    else {
      Diag(ExprLoc, diag::err_reflected_id_invalid_operand_type) << T;
      return true;
    }
  }

  Result = &Context.Idents.get(Buf);
  return false;
}

void Sema::ActOnCXXInvalidIdentifierSplice(IdentifierInfo *&Result) {
  BuildCXXInvalidIdentifierSplice(Result);
}

/// Constructs a new invalid identifier splice.
///
/// Returns true upon error.
void Sema::BuildCXXInvalidIdentifierSplice(IdentifierInfo *&Result) {
  Result = &Context.Idents.getInvalid(Context);
}

ExprResult Sema::ActOnCXXDependentSpliceIdExpression(
    CXXScopeSpec &SS, SourceLocation TemplateKWLoc,
    DeclarationNameInfo NameInfo, const TemplateArgumentListInfo *TemplateArgs,
    bool HasTrailingLParen, bool IsAddressOfOperand) {
  return BuildCXXDependentSpliceIdExpression(
      SS, TemplateKWLoc, NameInfo, TemplateArgs, HasTrailingLParen,
      IsAddressOfOperand);
}

ExprResult Sema::BuildCXXDependentSpliceIdExpression(
    CXXScopeSpec &SS, SourceLocation TemplateKWLoc,
    DeclarationNameInfo NameInfo, const TemplateArgumentListInfo *TemplateArgs,
    bool HasTrailingLParen, bool IsAddressOfOperand) {
  return CXXDependentSpliceIdExpr::Create(Context, NameInfo,
      SS, SS.getWithLocInContext(Context), TemplateKWLoc, HasTrailingLParen,
      IsAddressOfOperand, TemplateArgs);
}

static QualType BuildTypeForIdentifierSplice(
    Sema &SemaRef, CXXScopeSpec &SS, IdentifierInfo *Id,
    SourceLocation IdLoc, TemplateArgumentListInfo &TemplateArgs) {
  if (Id->isSplice()) {
    return SemaRef.getASTContext().getDependentIdentifierSpliceType(
        SS.getScopeRep(), Id, TemplateArgs);
  } else {
    return SemaRef.ResolveNonDependentIdentifierSpliceType(
        SemaRef.getCurScope(), SS, Id, IdLoc, TemplateArgs);
  }
}

QualType Sema::ResolveNonDependentIdentifierSpliceType(
    Scope *S, CXXScopeSpec &SS,
    IdentifierInfo *Id, SourceLocation IdLoc,
    TemplateArgumentListInfo &TemplateArgs) {
  LookupResult LookupResult(*this, Id, IdLoc, Sema::LookupOrdinaryName);
  LookupParsedName(LookupResult, S, &SS);

  if (!LookupResult.isSingleResult())
    return QualType();

  NamedDecl *FirstDecl = (*LookupResult.begin())->getUnderlyingDecl();
  if (auto *Type = dyn_cast<TypeDecl>(FirstDecl)) {
    DiagnoseUseOfDecl(Type, IdLoc);
    MarkAnyDeclReferenced(Type->getLocation(), Type, /*OdrUse=*/false);
    QualType Result = Context.getTypeDeclType(Type);

    if (SS.isNotEmpty())
      Result = getElaboratedType(ETK_None, SS, Result);

    return Result;
  }

  if (auto *Template = dyn_cast<TemplateDecl>(FirstDecl)) {
    // FIXME: This might not be right, is TemplateLoc the location of kw_template
    // or the template name loc? Is there something that requires this location?
    QualType Result = CheckTemplateIdType(
        TemplateName(Template), /*TemplateLoc=*/SourceLocation(), TemplateArgs);

    if (SS.isNotEmpty())
      Result = getElaboratedType(ETK_None, SS, Result);

    return Result;
  }

  Diag(IdLoc, diag::err_identifier_splice_not_type);
  Diag(FirstDecl->getLocation(), diag::note_identifier_splice_resolved_here)
      << FirstDecl;
  return QualType();
}

void Sema::BuildIdentifierSpliceTypeLoc(
    TypeLocBuilder &TLB, QualType T, SourceLocation TypenameLoc,
    NestedNameSpecifierLoc QualifierLoc,
    SourceLocation IdLoc, TemplateArgumentListInfo &TemplateArgs) {
  if (T->getAs<DependentIdentifierSpliceType>()) {
    DependentIdentifierSpliceTypeLoc NewTL
        = TLB.push<DependentIdentifierSpliceTypeLoc>(T);
    NewTL.setTypenameKeywordLoc(TypenameLoc);
    NewTL.setQualifierLoc(QualifierLoc);
    NewTL.setIdentifierLoc(IdLoc);
    NewTL.setLAngleLoc(TemplateArgs.getLAngleLoc());
    NewTL.setRAngleLoc(TemplateArgs.getRAngleLoc());
    for (unsigned I = 0; I < TemplateArgs.size(); ++I)
      NewTL.setArgLocInfo(I, TemplateArgs[I].getLocInfo());
  } else {
    const auto *ElabTy = T->getAs<ElaboratedType>();
    QualType InnerT = ElabTy ? ElabTy->getNamedType() : T;

    if (InnerT->getAs<TemplateSpecializationType>()) {
      TemplateSpecializationTypeLoc NewTL
        = TLB.push<TemplateSpecializationTypeLoc>(InnerT);
      NewTL.setTemplateKeywordLoc(SourceLocation());
      NewTL.setTemplateNameLoc(IdLoc);
      NewTL.setLAngleLoc(TemplateArgs.getLAngleLoc());
      NewTL.setRAngleLoc(TemplateArgs.getRAngleLoc());
      for (unsigned I = 0; I < TemplateArgs.size(); ++I)
        NewTL.setArgLocInfo(I, TemplateArgs[I].getLocInfo());
    } else {
      TLB.pushTypeSpec(InnerT).setNameLoc(IdLoc);
    }

    if (ElabTy) {
      // Push the outer elaborated type
      ElaboratedTypeLoc ElabTL = TLB.push<ElaboratedTypeLoc>(T);
      ElabTL.setElaboratedKeywordLoc(SourceLocation());
      ElabTL.setQualifierLoc(QualifierLoc);
    }
  }
}

TypeResult
Sema::ActOnCXXDependentIdentifierSpliceType(
    SourceLocation TypenameKWLoc, CXXScopeSpec &SS,
    IdentifierInfo *Id, SourceLocation IdLoc, SourceLocation LAngleLoc,
    ASTTemplateArgsPtr TemplateArgsIn, SourceLocation RAngleLoc) {
  TemplateArgumentListInfo TemplateArgs(LAngleLoc, RAngleLoc);
  translateTemplateArguments(TemplateArgsIn, TemplateArgs);

  QualType Result = BuildTypeForIdentifierSplice(
      *this, SS, Id, IdLoc, TemplateArgs);
  if (Result.isNull())
    return TypeResult(true);

  NestedNameSpecifierLoc QualifierLoc;
  if (SS.isNotEmpty())
    QualifierLoc = SS.getWithLocInContext(Context);

  // Create source-location information for this type.
  TypeLocBuilder Builder;
  BuildIdentifierSpliceTypeLoc(
      Builder, Result, TypenameKWLoc, QualifierLoc, IdLoc, TemplateArgs);

  return CreateParsedType(Result, Builder.getTypeSourceInfo(Context, Result));
}

/// Evaluates the given expression and yields the computed type.
QualType Sema::BuildReflectedType(SourceLocation TypenameLoc, Expr *E) {
  if (E->isTypeDependent() || E->isValueDependent())
    return Context.getReflectedType(E, Context.DependentTy);

  // The operand must be a reflection.
  if (!CheckReflectionOperand(*this, E))
    return QualType();

  Reflection Refl;
  if (EvaluateReflection(*this, E, Refl))
    return QualType();

  if (Refl.isInvalid()) {
    DiagnoseInvalidReflection(*this, E, Refl);
    return QualType();
  }

  if (!Refl.isType()) {
    Diag(E->getExprLoc(), diag::err_expression_not_type_reflection);
    return QualType();
  }

  // Get the type of the reflected entity.
  QualType Reflected = Refl.getAsType();

  return Context.getReflectedType(E, Reflected);
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

static ParsedTemplateArgument
BuildDependentTemplarg(SourceLocation KWLoc, Expr *ReflExpr) {
  void *OpaqueReflexpr = reinterpret_cast<void *>(ReflExpr);
  return ParsedTemplateArgument(ParsedTemplateArgument::Dependent,
                                OpaqueReflexpr, KWLoc);
}

static ParsedTemplateArgument
BuildReflectedTypeTemplarg(Sema &S, SourceLocation KWLoc,
                           Expr *ReflExpr, const QualType &QT) {
  llvm::outs() << "Templarg - Type\n";
  const Type *T = QT.getTypePtr();
  void *OpaqueT = reinterpret_cast<void *>(const_cast<Type *>(T));
  return ParsedTemplateArgument(ParsedTemplateArgument::Type, OpaqueT, KWLoc);
}

static ParsedTemplateArgument
BuildReflectedDeclTemplarg(Sema &S, SourceLocation KWLoc,
                           Expr *ReflExpr, const Decl *D) {
  llvm::outs() << "Templarg - Decl\n";
  if (const TemplateDecl *TDecl = dyn_cast<TemplateDecl>(D)) {
    using TemplateTy = OpaquePtr<TemplateName>;
    TemplateTy Template = TemplateTy::make(
        TemplateName(const_cast<TemplateDecl *>(TDecl)));

    return ParsedTemplateArgument(ParsedTemplateArgument::Template,
                                  Template.getAsOpaquePtr(),
                                  KWLoc);
  }

  llvm_unreachable("Unsupported reflected declaration type");
}

static ParsedTemplateArgument
BuildReflectedExprTemplarg(Sema &S, SourceLocation KWLoc,
                           Expr *E, const Expr *RE) {
  llvm::outs() << "Templarg - Expr\n";
  void *OpaqueRE = reinterpret_cast<void *>(const_cast<Expr *>(RE));
  return ParsedTemplateArgument(ParsedTemplateArgument::NonType, OpaqueRE,
                                KWLoc);
}

ParsedTemplateArgument
Sema::ActOnReflectedTemplateArgument(SourceLocation KWLoc, Expr *E) {
  if (E->isTypeDependent() || E->isValueDependent())
    return BuildDependentTemplarg(KWLoc, E);

  if (!CheckReflectionOperand(*this, E))
    return ParsedTemplateArgument();

  Reflection Refl;
  if (EvaluateReflection(*this, E, Refl))
    return ParsedTemplateArgument();

  switch (Refl.getKind()) {
  case RK_invalid:
    DiagnoseInvalidReflection(*this, E, Refl);
    return ParsedTemplateArgument();

  case RK_type: {
    const QualType QT = Refl.getAsType();
    return BuildReflectedTypeTemplarg(*this, KWLoc, E, QT);
  }

  case RK_declaration: {
    const Decl *D = Refl.getAsDeclaration();
    return BuildReflectedDeclTemplarg(*this, KWLoc, E, D);
  }

  case RK_expression: {
    const Expr *RE = Refl.getAsExpression();
    return BuildReflectedExprTemplarg(*this, KWLoc, E, RE);
  }

  case RK_base_specifier:
  case RK_fragment:
    break;
  }

  llvm_unreachable("Unsupported reflection type");
}

ExprResult Sema::ActOnCXXConcatenateExpr(SmallVectorImpl<Expr *>& Parts,
                                         SourceLocation KWLoc,
                                         SourceLocation LParenLoc,
                                         SourceLocation RParenLoc) {
  return BuildCXXConcatenateExpr(Parts, KWLoc);
}

static bool CheckConcatenateOperand(QualType T) {
  if (T.isCXXStringLiteralType())
    return true;
  if (T->isIntegerType())
    return true;

  return false;
}

static bool CheckConcatenateOperand(Sema &SemaRef, Expr *E) {
  QualType T = E->getType();
  if (CheckConcatenateOperand(T))
    return true;

  SemaRef.Diag(E->getExprLoc(), diag::err_concatenate_wrong_operand_type);
  return false;
}

ExprResult Sema::BuildCXXConcatenateExpr(SmallVectorImpl<Expr *>& Parts,
                                         SourceLocation KWLoc) {
  // The type of the expression is 'const char *'.
  QualType StrTy = Context.getPointerType(Context.getConstType(Context.CharTy));

  // Look for dependent arguments.
  for (Expr *E : Parts) {
    if (E->isTypeDependent() || E->isValueDependent())
      return new (Context) CXXConcatenateExpr(Context, StrTy, KWLoc, Parts);
  }

  // Convert operands to rvalues.
  SmallVector<Expr*, 4> Converted;
  for (Expr *E : Parts) {
    // Decay arrays first.
    ExprResult R = DefaultFunctionArrayLvalueConversion(E);
    if (R.isInvalid())
      return ExprError();
    E = R.get();

    // Check that the operand (type) is acceptable.
    if (!CheckConcatenateOperand(*this, E))
      return ExprError();

    Converted.push_back(E);
  }

  return new (Context) CXXConcatenateExpr(Context, StrTy, KWLoc, Converted);
}

DiagnosticsEngine &Sema::FakeParseScope::getDiagnostics() {
  return SemaRef.PP.getDiagnostics();
}
