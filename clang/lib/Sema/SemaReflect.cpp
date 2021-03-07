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

#include "TypeLocBuilder.h"
#include "clang/AST/ASTContext.h"
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
#include "clang/Sema/SemaInternal.h"
#include "clang/Sema/Template.h"

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
  assert(Reflection.isReflection());

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

static bool isReflectionOperand(Sema &SemaRef, Expr *E) {
  // Get the type of the expression.
  QualType Source = E->getType();
  if (Source->isReferenceType())
    Source = Source->getPointeeType();
  Source = Source.getUnqualifiedType();
  Source = SemaRef.Context.getCanonicalType(Source);

  return Source->isReflectionType();
}

// Check that the argument has the right type. Ignore references and
// cv-qualifiers on the expression.
static bool CheckReflectionOperand(Sema &SemaRef, Expr *E) {
  if (!isReflectionOperand(SemaRef, E)) {
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

template<template<typename> class Collection>
static bool hasDependentParts(Collection<Expr *>& Parts) {
 return std::any_of(Parts.begin(), Parts.end(), [](const Expr *E) {
   return E->isTypeDependent() || E->isValueDependent();
 });
}

ExprResult Sema::ActOnCXXReflectPrintLiteral(SourceLocation KWLoc,
                                             SmallVectorImpl<Expr *> &Args,
                                             SourceLocation LParenLoc,
                                             SourceLocation RParenLoc) {
  if (hasDependentParts(Args))
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

  Decl *D = const_cast<Decl *>(R.getAsDeclaration());

  // If we have any other kind of value decl, build a decl reference.
  if (auto *VD = dyn_cast<ValueDecl>(D)) {
    // Add a CXXScopeSpec to fields so that we get the correct pointer
    // types when addressing the spliced expression.
    CXXScopeSpec SS;
    if (auto *FD = dyn_cast<FieldDecl>(D)) {
      QualType RecordType = S.Context.getRecordType(FD->getParent());
      TypeSourceInfo *TSI = S.Context.getTrivialTypeSourceInfo(RecordType, SL);

      SS.Extend(S.Context, SourceLocation(), TSI->getTypeLoc(), SL);
    }

    QualType T = VD->getType();
    ValueDecl *NCVD = const_cast<ValueDecl *>(VD);
    ExprValueKind VK = S.getValueKindForDeclReference(T, NCVD, SL);
    return S.BuildDeclRefExpr(NCVD, T, VK, SL, SS.isEmpty() ? nullptr : &SS);
  }

  return nullptr;
}

static ConstantExpr *ReflectionToValueExpr(Sema &S, Expr *E) {
  // Ensure the expression results in an rvalue.
  ExprResult Res = S.DefaultFunctionArrayLvalueConversion(E);
  if (Res.isInvalid())
    return nullptr;
  E = Res.get();

  // Evaluate the resulting expression.
  SmallVector<PartialDiagnosticAt, 4> Diags;
  Expr::EvalResult Result;
  Result.Diag = &Diags;
  Expr::EvalContext EvalCtx(S.Context, S.GetReflectionCallbackObj());
  if (!E->EvaluateAsRValue(Result, EvalCtx)) {
    S.Diag(E->getExprLoc(), diag::reflection_reflects_non_constant_expression);
    for (PartialDiagnosticAt PD : Diags)
      S.Diag(PD.first, PD.second);
    return nullptr;
  }

  return ConstantExpr::Create(S.Context, const_cast<Expr *>(E),
                              std::move(Result.Val));
}

// FIXME: This is likely to aggressive and probably marks things used
// that aren't really used.
static DeclRefExpr *MarkDeclUsedInExprSplice(Sema &S, const DeclRefExpr *DRE) {
  Decl *ReferencedDecl = const_cast<NamedDecl *>(DRE->getFoundDecl());
  ReferencedDecl->setReferenced();
  ReferencedDecl->markUsed(S.Context);
  return const_cast<DeclRefExpr *>(DRE);
}

static ExprResult getSplicedExpr(Sema &SemaRef, Expr *Refl,
                                 bool AllowValue = false) {
  // The operand must be a reflection.
  if (!CheckReflectionOperand(SemaRef, Refl))
    return ExprError();

  SourceLocation ReflLoc = Refl->getExprLoc();

  Reflection R;
  if (EvaluateReflection(SemaRef, Refl, R))
    return ExprError();

  if (R.isInvalid()) {
    DiagnoseInvalidReflection(SemaRef, Refl, R);
    return ExprError();
  }

  if (R.isDeclaration())
    if (auto *DRE = DeclReflectionToDeclRefExpr(SemaRef, R, ReflLoc))
      return MarkDeclUsedInExprSplice(SemaRef, DRE);

  if (R.isExpression()) {
    Expr *E = const_cast<Expr *>(R.getAsExpression());

    if (auto *DRE = dyn_cast<DeclRefExpr>(E))
      return MarkDeclUsedInExprSplice(SemaRef, DRE);

    if (isa<UnresolvedLookupExpr>(E))
      return E;

    if (ConstantExpr *CE = ReflectionToValueExpr(SemaRef, E))
      return CE;
  }

  // FIXME: Emit a better error diagnostic.
  SemaRef.Diag(ReflLoc, diag::err_expression_not_value_reflection);
  return ExprError();
}

ExprResult Sema::ActOnCXXExprSpliceExpr(SourceLocation SBELoc,
                                        Expr *Reflection,
                                        SourceLocation SEELoc) {
  if (Reflection->isTypeDependent() || Reflection->isValueDependent())
    return CXXExprSpliceExpr::Create(Context, SBELoc, Reflection, SEELoc);

  return getSplicedExpr(*this, Reflection, /*AllowValue=*/true);
}

ExprResult Sema::ActOnCXXMemberExprSpliceExpr(
    Expr *Base, Expr *Refl, bool IsArrow,
    SourceLocation OpLoc, SourceLocation TemplateKWLoc,
    SourceLocation LParenLoc, SourceLocation RParenLoc,
    const TemplateArgumentListInfo *TemplateArgs) {
  if (Refl->isTypeDependent() || Refl->isValueDependent())
    return CXXMemberExprSpliceExpr::Create(
        Context, Base, Refl, IsArrow, OpLoc, TemplateKWLoc,
        LParenLoc, LParenLoc, TemplateArgs);

  // Get the reflection evaluated then reexpressed as a DeclRefExpr
  // or an UnresolvedLookupExpr.
  //
  // FIXME: We can refactor this to avoid the creation of this intermediary
  // expression
  ExprResult ReflectedExprResult = getSplicedExpr(*this, Refl);
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
      // Suppress access errors (including base class conversion errors)
      // whe splicing a member.
      //
      // TOOD: Do we need to do anything with this trap?
      // SFINAETrap Trap(*this, true);
      auto Prev = ForceBaseConversion;
      ForceBaseConversion = true;
      auto R = BuildFieldReferenceExpr(
          Base, IsArrow, OpLoc, /*SS*/{}, FD,
          DeclAccessPair::make(FD, FD->getAccess()), Named->getNameInfo());
      ForceBaseConversion = Prev;
      return R;
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

ExprResult Sema::ActOnCXXMemberExprSpliceExpr(
    Expr *Base, Expr *Refl, bool IsArrow,
    SourceLocation OpLoc, SourceLocation TemplateKWLoc,
    SourceLocation LParenLoc, SourceLocation RParenLoc,
    SourceLocation LAngleLoc, ASTTemplateArgsPtr TemplateArgsPtr,
    SourceLocation RAngleLoc) {
  TemplateArgumentListInfo TemplateArgs(LAngleLoc, RAngleLoc);
  if (translateTemplateArguments(TemplateArgsPtr, TemplateArgs))
    return ExprError();

  return ActOnCXXMemberExprSpliceExpr(
      Base, Refl, IsArrow, OpLoc, TemplateKWLoc,
      LParenLoc, RParenLoc, &TemplateArgs);
}

TemplateArgumentLoc Sema::ActOnCXXMysterySpliceTemplateArgument(
    SourceLocation ExpansionEllipsisLoc, SourceLocation SBELoc,
    Expr *Operand, SourceLocation SEELoc) {
  return BuildCXXMysterySpliceTemplateArgument(
      ExpansionEllipsisLoc, SBELoc, Operand, SEELoc);
}

TemplateArgumentLoc Sema::BuildCXXMysterySpliceTemplateArgument(
    SourceLocation ExpansionEllipsisLoc, SourceLocation SBELoc,
    Expr *Operand, SourceLocation SEELoc) {
  // FIXME: What do we do if there's an ellipsis loc?
  // Make a "PackSplice"?
  assert(ExpansionEllipsisLoc.isInvalid() && "not implemented");

  if (Operand->isTypeDependent() || Operand->isValueDependent()) {
    TemplateArgument Arg(TemplateArgument::Mystery, Operand);
    return TemplateArgumentLoc(Context, Arg, SBELoc, SEELoc);
  }

  if (!CheckReflectionOperand(*this, Operand))
    return TemplateArgumentLoc();

  // FIXME: This is duplicated by the handling of pack splices

  // FIXME: We have to evaluate the reflection expression twice,
  // once to figure out which splice to direct to, and once for the
  // actual splice expression.
  Reflection Refl;
  if (EvaluateReflection(*this, Operand, Refl))
    return TemplateArgumentLoc();

  switch (Refl.getKind()) {
  case RK_type: {
    TypeLocBuilder TLB;
    QualType Res = BuildTypeSpliceTypeLoc(
        TLB, /*TypenameKWLoc=*/SourceLocation(), SBELoc, Operand, SEELoc);
    if (Res.isNull())
      return TemplateArgumentLoc();

    return TemplateArgumentLoc(TemplateArgument(Res),
                               TLB.getTypeSourceInfo(Context, Res));
  }
  case RK_declaration: {
    if (auto *TD = dyn_cast<TemplateDecl>(Refl.getAsDeclaration())) {
      TemplateName N = Context.getSplicedTemplateReflection(Operand, TD);
      return TemplateArgumentLoc(Context, TemplateArgument(N),
                                 /*QualifierLoc=*/NestedNameSpecifierLoc(),
                                 /*TemplateNameLoc=*/SourceLocation());
    }
    [[clang::fallthrough]];
  }
  default: {
    ExprResult Res = ActOnCXXExprSpliceExpr(SBELoc, Operand, SEELoc);
    if (Res.isInvalid())
      return TemplateArgumentLoc();

    Expr *NE = Res.get();
    return TemplateArgumentLoc(TemplateArgument(NE), NE);
  }
  }
}

namespace {

enum RangeKind {
  RK_Array,
  RK_Range,
  RK_Tuple,
  RK_Struct,
  RK_Unknown,
};

/// A facility used to determine the begin and end of a constexpr range
/// or array.
struct ExpansionContextBuilder {
  ExpansionContextBuilder(Sema &S, Scope *CS, Expr *Range)
    : SemaRef(S), CurScope(CS), Range(Range)
    {}

  /// Construct calls to std::begin(Range) and std::end(Range)
  /// Returns true on error.
  bool BuildCalls();

  Expr *getRangeBeginCall() const { return RangeBegin; }
  Expr *getRangeEndCall() const { return RangeEnd; }

  Expr *getRange() const { return Range; }

  RangeKind getKind() const { return Kind; }
private:
  bool BuildArrayCalls();
  bool BuildRangeCalls();

private:
  Sema &SemaRef;

  /// The scope in which analysis will be performed.
  Scope *CurScope;

  /// Calls to std::begin(range) and std::end(range), respectively.
  Expr *RangeBegin = nullptr;
  Expr *RangeEnd = nullptr;

  /// The Range that we are constructing an expansion context from.
  Expr *Range;

  RangeKind Kind;
};

bool
ExpansionContextBuilder::BuildCalls()
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
ExpansionContextBuilder::BuildArrayCalls()
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

/// Traverse a C++ Constexpr Range
struct RangeTraverser {
  RangeTraverser(Sema &SemaRef, RangeKind Kind, Expr *RangeBegin,
                 Expr *RangeEnd)
    : SemaRef(SemaRef), Current(RangeBegin), RangeEnd(RangeEnd),
    Kind(Kind), I()
    {}

  /// Current == RangeEnd
  explicit operator bool();

  /// Dereference and evaluate the current value as a constant expression.
  Expr *operator*();

  /// Call std::next(Current, 1) if this is a constexpr range,
  /// Increment the array subscript if it is an array.
  RangeTraverser &operator++();

  /// Get the range kind.
  RangeKind getKind() const { return Kind; }

private:
  Sema &SemaRef;

  /// The current element in the traversal
  Expr *Current;

  /// One-past-the-end iterator (std::end) of the range we are traversing.
  Expr *RangeEnd = nullptr;

  /// The kind of product type we are traversing.
  RangeKind Kind;

  /// An integer Index that keeps track of the current element.
  std::size_t I;
};

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
RangeTraverser::operator bool()
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

    Expr::EvalContext EvalCtx(SemaRef.Context, SemaRef.GetReflectionCallbackObj());
    if (!EqualExpr->EvaluateAsConstantExpr(EqualRes, EvalCtx)) {
      SemaRef.Diag(SourceLocation(), diag::err_constexpr_range_iteration_failed);
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
RangeTraverser::operator*()
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

RangeTraverser &
RangeTraverser::operator++()
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

}

ExprResult Sema::ActOnCXXPackSpliceExpr(SourceLocation EllipsisLoc,
                                        SourceLocation SBELoc,
                                        Expr *Operand,
                                        SourceLocation SEELoc) {
  PackSplice *PS = ActOnCXXPackSplice(getCurScope(), Operand);
  if (!PS)
    return ExprError();

  return BuildCXXPackSpliceExpr(PS, EllipsisLoc, SBELoc, SEELoc);
}

ExprResult Sema::BuildCXXPackSpliceExpr(const PackSplice *PS,
                                        SourceLocation EllipsisLoc,
                                        SourceLocation SBELoc,
                                        SourceLocation SEELoc) {
  return CXXPackSpliceExpr::Create(Context, PS, EllipsisLoc, SBELoc, SEELoc);
}

TemplateArgumentLoc Sema::ActOnCXXPackSpliceTemplateArgument(
                           Expr *Operand, SourceLocation ExpansionEllipsisLoc) {
  PackSplice *PS = ActOnCXXPackSplice(getCurScope(), Operand);
  if (!PS)
    return TemplateArgumentLoc();

  if (ExpansionEllipsisLoc.isInvalid()) {
    // FIXME: Should this be part of a more general mechanism?
    //
    // We can get away with doing this here as this is a rather
    // special case, and we don't need to worry about a template
    // argument pack splice being expanded at a later point.
    Diag(Operand->getBeginLoc(), diag::err_unexpanded_pack_splice);
    return TemplateArgumentLoc();
  }

  return BuildCXXPackSpliceTemplateArgument(PS, ExpansionEllipsisLoc);
}

TemplateArgumentLoc Sema::BuildCXXPackSpliceTemplateArgument(
                          PackSplice *PS, SourceLocation ExpansionEllipsisLoc) {
  TemplateArgument TArg(PS, /*IsPattern=*/ExpansionEllipsisLoc.isInvalid());
  return TemplateArgumentLoc(Context, TArg,
                             /*IntroEllipsisLoc=*/SourceLocation(),
                             /*SBELoc=*/SourceLocation(),
                             /*SEELoc=*/SourceLocation(),
                             ExpansionEllipsisLoc);
}

PackSplice *Sema::ActOnCXXPackSplice(Scope *S, Expr *Operand) {
  if (!isRangeSpliceEnabled()) {
    // FIXME: Implement diagnostic, possibly see if this would have
    // succeeded and report that as part of the diagnostic.
    llvm_unreachable("diagnose, tried to form pack splice, "
                     "but range splices not valid here");
  }

  return BuildUnresolvedCXXPackSplice(S, Operand);
}

PackSplice *Sema::BuildUnresolvedCXXPackSplice(Scope *S, Expr *Operand) {
  ExpansionContextBuilder CtxBldr(*this, S, Operand);
  if (CtxBldr.BuildCalls()) {
    Diag(Operand->getBeginLoc(), diag::err_expression_unexpandable);
    return nullptr;
  }

  if (CtxBldr.getKind() == RK_Unknown)
    return PackSplice::Create(Context, Operand);

  SmallVector<Expr *, 8> Exprs;

  RangeTraverser RT(*this, CtxBldr.getKind(),
                    CtxBldr.getRangeBeginCall(), CtxBldr.getRangeEndCall());
  while (!RT) {
    Exprs.push_back(*RT);
    ++RT;
  }

  return BuildResolvedCXXPackSplice(Operand, Exprs);
}

PackSplice *Sema::BuildResolvedCXXPackSplice(Expr *Operand,
                                             ArrayRef<Expr *> Expansions) {
  return PackSplice::Create(Context, Operand, Expansions);
}

bool Sema::CheckCXXPackSpliceForExpansion(const PackSplice *PS,
                                          bool &ShouldExpand,
                                          Optional<unsigned> &NumExpansions) {
  assert(PS->isExpanded() && "the splice must first be expanded");

  if (PS->hasDependentOperands()) {
    ShouldExpand = false;
    return false;
  }

  NumExpansions = PS->getNumExpansions();

  return false;
}

Optional<unsigned> Sema::getNumArgumentsInExpansion(const PackSplice *PS) {
  bool IgnoredShouldExpand = true;

  Optional<unsigned> NumExpansions;
  if (CheckCXXPackSpliceForExpansion(PS, IgnoredShouldExpand, NumExpansions))
    return {};

  return NumExpansions;
}

static bool ExpandCXXPackSplice(Sema &SemaRef, PackSpliceLoc PSLoc,
                                TemplateArgumentLoc &TemplArg) {
  assert(SemaRef.ArgumentPackSubstitutionIndex != -1);

  const PackSplice *PS = PSLoc.getPackSplice();
  assert(PS->isExpanded());

  ASTContext &Ctx = SemaRef.getASTContext();

  Expr *RTE = PS->getExpansion(SemaRef.ArgumentPackSubstitutionIndex);
  // FIXME: We have to evaluate the reflection expression twice,
  // once to figure out which splice to direct to, and once for the
  // actual splice expression.
  Reflection Refl;
  if (EvaluateReflection(SemaRef, RTE, Refl))
    return true;

  // FIXME: Handle template template arguments
  switch (Refl.getKind()) {
  case RK_type: {
    // FIXME: This can likely be migrated to BuildTypeSpliceTypeLoc
    // and then we can drop SubstTypePackSpliceType; (Should we?)
    QualType Res = SemaRef.BuildTypeSpliceType(RTE);
    if (Res.isNull())
      return true;

    // Wrap the spliced type
    Res = Ctx.getSubstTypePackSpliceType(RTE, Res);

    // FIXME: This is a bit unfortunate, similar to the double evaluation
    // in the case of an expansion as a type (ExpandCXXPackSpliceAsType)
    // we end up building this TypeLoc twice. Once here, then once
    // in the transform function.
    TypeLocBuilder TLB;
    auto NewTL = TLB.push<SubstTypePackSpliceTypeLoc>(Res);
    NewTL.setEllipsisLoc(PSLoc.getEllipsisLoc());
    NewTL.setSBELoc(PSLoc.getSBELoc());
    NewTL.setSEELoc(PSLoc.getSEELoc());

    TemplArg = TemplateArgumentLoc(TemplateArgument(Res),
                                   TLB.getTypeSourceInfo(Ctx, Res));
    return false;
  }
  default: {
    SourceLocation SBELoc = PSLoc.getSBELoc();
    SourceLocation SEELoc = PSLoc.getSEELoc();

    ExprResult Res = SemaRef.ActOnCXXExprSpliceExpr(SBELoc, RTE, SEELoc);
    if (Res.isInvalid())
      return true;

    Expr *NE = Res.get();
    TemplArg = TemplateArgumentLoc(TemplateArgument(NE), NE);
    return false;
  }
  }
}

bool Sema::ExpandCXXPackSplice(const TemplateArgumentLoc &Input,
                               TemplateArgumentLoc &Output) {
  assert(Input.getArgument().isPackSplice() && "Unexpected kind");
  return ::ExpandCXXPackSplice(*this, Input.getAsPackSpliceLoc(), Output);
}

ExprResult Sema::ExpandCXXPackSpliceAsExpr(CXXPackSpliceExpr *E) {
  TemplateArgumentLoc TemplArg;
  if (::ExpandCXXPackSplice(*this, E->getAsPackSpliceLoc(), TemplArg))
    return ExprError();

  TemplateArgument RawArg = TemplArg.getArgument();
  if (RawArg.getKind() != TemplateArgument::Expression) {
    Diag(E->getBeginLoc(), diag::err_pack_splice_mismatch)
        << 0 << ArgumentPackSubstitutionIndex;
    return ExprError();
  }

  return RawArg.getAsExpr();
}

// This method unlike the previous expansion methods takes locations
// directly, then forms the PackSpliceLoc itself, as this prevents us
// from needing to setup and build a TypePackSpliceTypeLoc.
QualType Sema::ExpandCXXPackSpliceAsType(const TypePackSpliceType *T,
                                         SourceLocation EllipsisLoc,
                                         SourceLocation SBELoc,
                                         SourceLocation SEELoc) {
  TemplateArgumentLoc TemplArg;
  PackSpliceLoc PSLoc(T->getPackSplice(), EllipsisLoc, SBELoc, SEELoc);
  if (::ExpandCXXPackSplice(*this, PSLoc, TemplArg))
    return QualType();

  TemplateArgument RawArg = TemplArg.getArgument();
  if (RawArg.getKind() != TemplateArgument::Type) {
    Diag(SBELoc, diag::err_pack_splice_mismatch)
        << 1 << ArgumentPackSubstitutionIndex;
    return QualType();
  }

  return RawArg.getAsType();
}

namespace {

class ExpansionCodeSynthesisContext {
  Sema &SemaRef;
public:
  ExpansionCodeSynthesisContext(Sema &SemaRef,
                                SourceLocation PointOfInstantiation,
                                SourceRange InstantiationRange)
      : SemaRef(SemaRef) {
    Sema::CodeSynthesisContext Ctx;
    Ctx.Kind = Sema::CodeSynthesisContext::NonDependentPackSplicing;
    Ctx.PointOfInstantiation = PointOfInstantiation;
    Ctx.InstantiationRange = InstantiationRange;
    SemaRef.pushCodeSynthesisContext(Ctx);
  }

  ~ExpansionCodeSynthesisContext() {
    SemaRef.popCodeSynthesisContext();
  }
};

}

bool Sema::ExpandNonDependentPacks(const TemplateArgumentLoc *Inputs,
                                   unsigned NumInputs,
                                   SourceLocation PointOfInstantiation,
                                   SourceRange InstantiationRange,
                                   TemplateArgumentListInfo &Outputs) {
  ExpansionCodeSynthesisContext SynthContext(*this, PointOfInstantiation,
                                             InstantiationRange);

  MultiLevelTemplateArgumentList TemplateArgs;
  return SubstTemplateArguments({Inputs, NumInputs}, TemplateArgs, Outputs);
}

bool Sema::tryExpandNonDependentPack(const TemplateArgumentLoc &Input,
                                     TemplateArgumentListInfo &Outputs) {
  const TemplateArgument &Arg = Input.getArgument();
  if (!Arg.isPackExpansion() || Arg.isDependent()) {
    Outputs.addArgument(Input);
    return false;
  }

  return ExpandNonDependentPacks(&Input, /*NumInputs=*/1,
        /*PointOfInstantiation=*/Input.getExpansionEllipsisLoc(),
          /*InstantiationRange=*/Input.getSourceRange(), Outputs);
}

bool Sema::ExpandNonDependentPacks(Expr *const *Inputs, unsigned NumInputs,
                                   bool IsCall,
                                   SourceLocation PointOfInstantiation,
                                   SourceRange InstantiationRange,
                                   SmallVectorImpl<Expr *> &Outputs) {
  ExpansionCodeSynthesisContext SynthContext(*this, PointOfInstantiation,
                                             InstantiationRange);

  MultiLevelTemplateArgumentList TemplateArgs;
  return SubstExprs({Inputs, NumInputs}, IsCall, TemplateArgs, Outputs);
}

bool Sema::tryExpandNonDependentPack(Expr *Input, bool IsCall,
                                     SmallVectorImpl<Expr *> &Outputs) {
  if (!isa<PackExpansionExpr>(Input) || Input->getDependence()) {
    Outputs.push_back(Input);
    return false;
  }

  PackExpansionExpr *PE = cast<PackExpansionExpr>(Input);
  return ExpandNonDependentPacks(&Input, /*NumInputs=*/1, IsCall,
        /*PointOfInstantiation=*/PE->getEllipsisLoc(),
          /*InstantiationRange=*/PE->getSourceRange(), Outputs);
}

bool Sema::ExpandNonDependentPacks(CXXRecordDecl *Class,
                                   CXXBaseSpecifier *Inputs, unsigned NumInputs,
                                   SourceLocation PointOfInstantiation,
                                   SourceRange InstantiationRange,
                                 SmallVectorImpl<CXXBaseSpecifier *> &Outputs) {
  ExpansionCodeSynthesisContext SynthContext(*this, PointOfInstantiation,
                                             InstantiationRange);

  MultiLevelTemplateArgumentList TemplateArgs;
  return SubstBaseSpecifiers(Class, Inputs, NumInputs, TemplateArgs, Outputs);
}

static bool isBaseSpecifierExpandable(CXXBaseSpecifier *Input) {
  if (!Input->isPackExpansion())
    return false;

  QualType Ty = Input->getType();
  if (Ty->getDependence() & ~TypeDependence::UnexpandedPack)
    return false;

  return true;
}

bool Sema::tryExpandNonDependentPack(Decl *Class, CXXBaseSpecifier *Input,
                                 SmallVectorImpl<CXXBaseSpecifier *> &Outputs) {
  AdjustDeclIfTemplate(Class);

  if (!isBaseSpecifierExpandable(Input)) {
    Outputs.push_back(Input);
    return false;
  }

  return ExpandNonDependentPacks(cast<CXXRecordDecl>(Class), Input,
                   /*NumInputs=*/1,
        /*PointOfInstantiation=*/Input->getEllipsisLoc(),
          /*InstantiationRange=*/Input->getSourceRange(), Outputs);
}

bool Sema::ExpandNonDependentPacks(CXXConstructorDecl *Ctor,
                                   CXXCtorInitializer *const *Inputs,
                                   unsigned NumInputs,
                                   SourceLocation PointOfInstantiation,
                                   SourceRange InstantiationRange,
                               SmallVectorImpl<CXXCtorInitializer *> &Outputs) {
  ExpansionCodeSynthesisContext SynthContext(*this, PointOfInstantiation,
                                             InstantiationRange);

  MultiLevelTemplateArgumentList TemplateArgs;
  return SubstMemInitializers(Ctor, Inputs, NumInputs, TemplateArgs, Outputs);
}

static bool isInitializerExpandable(CXXCtorInitializer *Input) {
  if (!Input->isPackExpansion())
    return false;

  const Type *Ty = Input->getBaseClass();
  if (Ty->getDependence() & ~TypeDependence::UnexpandedPack)
    return false;

  return true;
}

bool Sema::tryExpandNonDependentPack(Decl *Ctor,
                                     CXXCtorInitializer *Input,
                               SmallVectorImpl<CXXCtorInitializer *> &Outputs) {
  if (!isInitializerExpandable(Input)) {
    Outputs.push_back(Input);
    return false;
  }

  return ExpandNonDependentPacks(cast<CXXConstructorDecl>(Ctor), &Input,
                   /*NumInputs=*/1,
        /*PointOfInstantiation=*/Input->getEllipsisLoc(),
          /*InstantiationRange=*/Input->getSourceRange(), Outputs);
}

static bool isParameterExpandable(ParmVarDecl *Input) {
  if (!Input->isParameterPack())
    return false;

  QualType Ty = Input->getType();
  if (Ty->getDependence() & ~TypeDependence::UnexpandedPack)
    return false;

  return true;
}

bool Sema::ExpandNonDependentPacks(LateParsedMethodParameterInfo &ParamPack,
                                   ParmVarDecl *const *Inputs,
                                   unsigned NumInputs,
                                   SourceLocation PointOfInstantiation,
                                   SourceRange InstantiationRange,
                                   SmallVectorImpl<ParmVarDecl *> &Outputs) {
  ExpansionCodeSynthesisContext SynthContext(*this, PointOfInstantiation,
                                             InstantiationRange);

  // Swap to the provided instantiation scope.
  assert(ParamPack.OriginalScope == CurrentInstantiationScope);
  LocalInstantiationScope *OldScope = CurrentInstantiationScope;
  CurrentInstantiationScope = ParamPack.Scope;

  // Push any ParmVarDecls we're about to expand.
  for (unsigned I = 0; I != NumInputs; ++I) {
    ParmVarDecl *Input = Inputs[I];

    // If the parameter is non-dependently expandable, add it to the
    // ExpandedDecls, we're guaranteed to expand it in a moment.
    //
    // Doing this prevents invasive integration with TreeTransform.
    if (isParameterExpandable(Input))
      ParamPack.ExpandedDecls.push_back(Input);
  }

  ArrayRef<ParmVarDecl *> InputsAsArrRef(Inputs, NumInputs);
  SmallVector<QualType, 16> OutputTypes;
  Sema::ExtParameterInfoBuilder ParamInfoBuilder;
  MultiLevelTemplateArgumentList TemplateArgs;

  bool Result = SubstParmTypes(SourceLocation(),
                               InputsAsArrRef,
                               /*ExtParamInfos=*/nullptr,
                               TemplateArgs,
                               OutputTypes, &Outputs,
                               ParamInfoBuilder);

  // Store the updated scope, and restore to the original scope.
  ParamPack.Scope = CurrentInstantiationScope->cloneScopes(OldScope);
  LocalInstantiationScope::deleteScopes(CurrentInstantiationScope, OldScope);

  return Result;
}

bool Sema::tryExpandNonDependentPack(IdentifierInfo *ParamII,
                                     SourceLocation ParamIILoc,
                                     std::unique_ptr<CachedTokens> DefArgToks,
                                     ParmVarDecl *Input,
                         SmallVectorImpl<DeclaratorChunk::ParamInfo> &Outputs) {
  bool IsExpandable = isParameterExpandable(Input);
  assert((!IsExpandable || !DefArgToks) &&
         "parameter packs cannot have default arguments");

  if (!IsExpandable) {
    Outputs.push_back(DeclaratorChunk::ParamInfo(ParamII, ParamIILoc,
                                                 Input, std::move(DefArgToks)));
    return false;
  }

  // Setup a scope if one is not already present.
  LateParsedMethodParameterInfo &Inf = LateMethodParameterInfoStack.back();
  if (!Inf.Scope) {
    LocalInstantiationScope *OldScope = CurrentInstantiationScope;
    LocalInstantiationScope Scope(*this);
    Inf.Scope = CurrentInstantiationScope->cloneScopes(OldScope);
  }

  TypeLoc GenericTypeLoc = Input->getTypeSourceInfo()->getTypeLoc();
  auto TypeLoc = GenericTypeLoc.getAs<PackExpansionTypeLoc>();

  SmallVector<ParmVarDecl *, 16> Params;
  if (ExpandNonDependentPacks(Inf, &Input, /*NumInputs=*/1,
     /*PointOfInstantiation=*/TypeLoc.getEllipsisLoc(),
       /*InstantiationRange=*/TypeLoc.getLocalSourceRange(), Params))
    return true;

  for (ParmVarDecl *Param : Params)
    Outputs.push_back(DeclaratorChunk::ParamInfo(nullptr, SourceLocation(),
                                                 Param, nullptr));

  return false;
}

bool Sema::ActOnCXXIdentifierSplice(
    ArrayRef<Expr *> Parts, IdentifierInfo *&Result) {
  return BuildCXXIdentifierSplice(Parts, Result);
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

/// Constructs a new identifier from the expressions in Parts.
///
/// Returns true upon error.
bool Sema::BuildCXXIdentifierSplice(
    ArrayRef<Expr *> Parts, IdentifierInfo *&Result) {
  // Perform some early checks so we can report problems early.
  for (std::size_t I = 0; I < Parts.size(); ++I) {
    Expr *E = Parts[I];

    // Get the type of the reflection.
    QualType T = DeduceCanonicalType(*this, E);
    if (T->isDependentType())
      continue;

    if (T->isConstantArrayType())
      continue;

    if (T->isPointerType())
      continue;

    SourceLocation ExprLoc = E->getBeginLoc();
    if (T->isIntegerType()) {
      if (I == 0) {
        // An identifier cannot start with an integer value.
        Diag(ExprLoc, diag::err_reflected_id_with_integer_prefix);
        return true;
      }

      continue;
    }

    Diag(ExprLoc, diag::err_reflected_id_invalid_operand_type) << T;
    return true;
  }

  // If any components are dependent, we can't compute the name.
  if (hasDependentParts(Parts)) {
    Result = &Context.Idents.get(Context, Parts);
    return false;
  }

  // Actually compute the name.
  SmallString<256> Buf;
  llvm::raw_svector_ostream OS(Buf);
  for (Expr *E : Parts) {
    assert(!E->isTypeDependent() && !E->isValueDependent()
        && "Dependent name component");

    // Get the type of the reflection.
    QualType T = DeduceCanonicalType(*this, E);

    // Evaluate the sub-expression (depending on type) in order to compute
    // a string part that will constitute a declaration name.
    if (T->isConstantArrayType()) {
      if (!AppendCharacterArray(*this, OS, E, T))
        return true;
      continue;
    }

    if (T->isPointerType()) {
      if (!AppendCharacterPointer(*this, OS, E, T))
        return true;
      continue;
    }

    if (T->isIntegerType()) {
      if (!AppendInteger(*this, OS, E, T))
        return true;
      continue;
    }

    llvm_unreachable("unsupported type, but wasn't caught above?");
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
  if (const auto *DependentTy = T->getAs<DependentIdentifierSpliceType>()) {
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

    if (const auto *SpecTy = InnerT->getAs<TemplateSpecializationType>()) {
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
  if (translateTemplateArguments(TemplateArgsIn, TemplateArgs))
    return TypeResult(true);

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

bool Sema::ActOnCXXNestedNameSpecifierSplice(CXXScopeSpec &SS, ExprResult E,
                                             SourceLocation ColonColonLoc) {
  assert(false && "Splice in nested-name-specifier");
}

bool Sema::ActOnCXXNestedNameSpecifierTypeSplice(CXXScopeSpec &SS,
                                                 const DeclSpec &DS,
                                                 SourceLocation ColonColonLoc) {
  if (SS.isInvalid() || DS.getTypeSpecType() == DeclSpec::TST_error)
    return true;

  assert(DS.getTypeSpecType() == DeclSpec::TST_type_splice);

  TypeLocBuilder TLB;
  // FIXME: Provide source location information for SBELoc and SEELoc
  QualType T = BuildTypeSpliceTypeLoc(
      TLB, DS.getTypeSpecTypeLoc(), /*SBELoc=*/SourceLocation(),
      DS.getRepAsExpr(), /*SEELoc=*/SourceLocation());
  if (T.isNull())
    return true;

  if (!T->isDependentType() && !T->getAs<TagType>()) {
    Diag(DS.getTypeSpecTypeLoc(), diag::err_expected_class_or_namespace)
      << T << getLangOpts().CPlusPlus;
    return true;
  }

  SS.Extend(Context, SourceLocation(), TLB.getTypeLocInContext(Context, T),
            ColonColonLoc);
  return false;
}

QualType Sema::ActOnTypeSpliceType(Expr *Operand) {
  return BuildTypeSpliceType(Operand);
}

/// Evaluates the given expression and yields the computed type.
QualType Sema::BuildTypeSpliceType(Expr *E) {
  if (E->isTypeDependent() || E->isValueDependent())
    return Context.getTypeSpliceType(E, Context.DependentTy);

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

  return Context.getTypeSpliceType(E, Reflected);
}

QualType Sema::BuildTypeSpliceTypeLoc(
    TypeLocBuilder &TLB, SourceLocation TypenameKWLoc,
    SourceLocation SBELoc, Expr *E, SourceLocation SEELoc) {
  QualType SpliceTy = BuildTypeSpliceType(E);

  auto TL = TLB.push<TypeSpliceTypeLoc>(SpliceTy);
  TL.setTypenameKeywordLoc(TypenameKWLoc);
  TL.setSBELoc(SBELoc);
  TL.setSEELoc(SEELoc);

  return SpliceTy;
}

QualType Sema::ActOnTypePackSpliceType(Scope *S, Expr *Operand) {
  PackSplice *PS = ActOnCXXPackSplice(getCurScope(), Operand);
  if (!PS)
    return QualType();

  return BuildTypePackSpliceType(PS);
}

QualType Sema::BuildTypePackSpliceTypeLoc(
    TypeLocBuilder &TLB, Scope *S, SourceLocation IntroEllipsisLoc,
    SourceLocation TemplateKWLoc, SourceLocation SBELoc,
    Expr *Operand, SourceLocation SEELoc) {
  QualType SpliceTy = ActOnTypePackSpliceType(S, Operand);

  auto TL = TLB.push<TypePackSpliceTypeLoc>(SpliceTy);
  TL.setEllipsisLoc(IntroEllipsisLoc);
  // TL.setTypenameKeywordLoc(TypenameLoc);
  TL.setSBELoc(SBELoc);
  TL.setSEELoc(SEELoc);

  return SpliceTy;
}

QualType Sema::BuildTypePackSpliceType(const PackSplice *PS) {
  return Context.getTypePackSpliceType(PS);
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

// This is called from when analyzing typename-specifiers (generally when
// optionally parsing nested-name-specifiers) to determine that a type splice
// is valid. For example:
//
//    typename [:^int:]
//    typename [:^some_class:]::type
//
// If the splice operand is dependent, then this is a dependent type.
TypeResult Sema::ActOnCXXTypenameSpecifierSpliceType(Scope *S, Expr *Operand) {
  TypeLocBuilder TLB;
  QualType SpliceTy = BuildCXXTypenameSpecifierSpliceTypeLoc(
      TLB, S, RangeSpliceIntroEllipsisLoc, /*TemplateKWLoc=*/SourceLocation(),
      /*SBELoc=*/SourceLocation(), Operand, /*SEELoc=*/SourceLocation());
  if (SpliceTy.isNull())
    return TypeError();

  return CreateParsedType(SpliceTy, TLB.getTypeSourceInfo(Context, SpliceTy));
}

// Internal function for building a dependent typename specifier
// splice type loc
QualType Sema::BuildCXXTypenameSpecifierSpliceTypeLoc(
    TypeLocBuilder &TLB, SourceLocation IntroEllipsisLoc,
    SourceLocation TypenameKWLoc, SourceLocation SBELoc,
    Expr *Operand, SourceLocation SEELoc) {
  QualType SpliceTy = Context.getTypenameSpecifierSpliceType(Operand);

  auto TL = TLB.push<TypenameSpecifierSpliceTypeLoc>(SpliceTy);
  TL.setIntroEllipsisLoc(IntroEllipsisLoc);
  TL.setTypenameKeywordLoc(TypenameKWLoc);
  TL.setSBELoc(SBELoc);
  TL.setSEELoc(SEELoc);

  return SpliceTy;
}

QualType Sema::BuildCXXTypenameSpecifierSpliceTypeLoc(
    TypeLocBuilder &TLB, Scope *S, SourceLocation IntroEllipsisLoc,
    SourceLocation TypenameKWLoc, SourceLocation SBELoc,
    Expr *Operand, SourceLocation SEELoc) {
  // If the expression is dependent, the type is dependent.
  if (Operand->isTypeDependent())
    return BuildCXXTypenameSpecifierSpliceTypeLoc(
        TLB, IntroEllipsisLoc, TypenameKWLoc, SBELoc, Operand, SEELoc);

  if (isReflectionOperand(*this, Operand))
    return BuildTypeSpliceTypeLoc(TLB, TypenameKWLoc, SBELoc, Operand, SEELoc);

  // Since this is not dependent, and not a reflection operand,
  // try to form a pack splice.
  return BuildTypePackSpliceTypeLoc(
      TLB, S, IntroEllipsisLoc, TypenameKWLoc, SBELoc, Operand, SEELoc);
}

