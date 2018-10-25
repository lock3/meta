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
#include "clang/Sema/ParsedTemplate.h"
#include "clang/Sema/ParsedReflection.h"
#include "clang/Sema/Scope.h"
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

  llvm::outs() << "GOT TYPE\n";
  T.get().get()->dump();
  return ParsedReflectionOperand(T.get(), Arg.getLocation());
}

ParsedReflectionOperand Sema::ActOnReflectedTemplate(ParsedTemplateArgument T) {
  assert(T.getKind() == ParsedTemplateArgument::Template);
  const CXXScopeSpec& SS = T.getScopeSpec();
  ParsedTemplateTy Temp = T.getAsTemplate();
  
  llvm::outs() << "GOT TEMPLATE\n";
  Temp.get().getAsTemplateDecl()->dump();
  return ParsedReflectionOperand(SS, Temp, T.getLocation());
}

ParsedReflectionOperand Sema::ActOnReflectedNamespace(CXXScopeSpec &SS,
                                                      SourceLocation &Loc,
                                                      Decl *D) {
  llvm::outs() << "GOT NAMESPACE\n";
  D->dump();
  return ParsedReflectionOperand(SS, D, Loc);
}

ParsedReflectionOperand Sema::ActOnReflectedExpression(Expr *E) {
  
  llvm::outs() << "GOT EXPRESSION\n";
  E->dump();
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
  case ParsedReflectionOperand::Namespace: {
    // FIXME: If the ScopeSpec is non-empty, create a qualified namespace-name.
    NamespaceName Arg(cast<NamespaceDecl>(Ref.getAsNamespace()));
    return BuildCXXReflectExpr(Loc, Arg, LP, RP);
  }
  case ParsedReflectionOperand::Expression: {
    Expr *Arg = Ref.getAsExpr();
    return BuildCXXReflectExpr(Loc, Arg, LP, RP);
  }
  }

  return ExprError();
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

static bool SetType(QualType& Ret, QualType T) {
  Ret = T;
  return true;
}

static bool SetCStrType(QualType& Ret, ASTContext &Ctx) {
  return SetType(Ret, Ctx.getPointerType(Ctx.getConstType(Ctx.CharTy)));
}

// Gets the type and query from Arg. Returns false on error.
static bool GetTypeAndQuery(Sema &SemaRef, Expr *&Arg, QualType &ResultTy, 
                            ReflectionQuery& Query) {
  // Guess these values initially.
  ResultTy = SemaRef.Context.DependentTy;
  Query = RQ_unknown;

  if (Arg->isTypeDependent() || Arg->isValueDependent())
    return true;

  // Convert to an rvalue.
  ExprResult Conv = SemaRef.DefaultLvalueConversion(Arg);
  if (Conv.isInvalid())
    return false;
  Arg = Conv.get();

  // FIXME: Should what type should this actually be?
  QualType T = Arg->getType();
  if (T != SemaRef.Context.IntTy) {
    SemaRef.Diag(Arg->getExprLoc(), diag::err_reflection_query_wrong_type);
    return false;
  }

  // Evaluate the query operand
  SmallVector<PartialDiagnosticAt, 4> Diags;
  Expr::EvalResult Result;
  Result.Diag = &Diags;
  if (!Arg->EvaluateAsRValue(Result, SemaRef.Context)) {
    // FIXME: This is not the right error.
    SemaRef.Diag(Arg->getExprLoc(), diag::err_reflection_query_not_constant);
    for (PartialDiagnosticAt PD : Diags)
      SemaRef.Diag(PD.first, PD.second);
    return false;
  }

  Query = static_cast<ReflectionQuery>(Result.Val.getInt().getExtValue());

  if (isPredicateQuery(Query)) 
    return SetType(ResultTy, SemaRef.Context.BoolTy);
  if (isTraitQuery(Query))
    return SetType(ResultTy, SemaRef.Context.UnsignedIntTy);
  if (isAssociatedReflectionQuery(Query))
    return SetType(ResultTy, SemaRef.Context.MetaInfoTy);
  if (isNameQuery(Query))
    return SetCStrType(ResultTy, SemaRef.Context);

  SemaRef.Diag(Arg->getExprLoc(), diag::err_reflection_query_invalid);
  return false;
}

ExprResult Sema::ActOnCXXReflectionTrait(SourceLocation KWLoc,
                                         SmallVectorImpl<Expr *> &Args,
                                         SourceLocation LParenLoc,
                                         SourceLocation RParenLoc) {
  // FIXME: There are likely unary and n-ary queries.
  if (Args.size() != 2) {
    Diag(KWLoc, diag::err_reflection_wrong_arity) << (int)Args.size();
    return ExprError();
  }

  // Get the type of the query. Note that this will convert Args[0].
  QualType Ty;
  ReflectionQuery Query;
  if (!GetTypeAndQuery(*this, Args[0], Ty, Query))
    return ExprError();

  // Convert the remaining operands to rvalues.
  for (std::size_t I = 1; I < Args.size(); ++I) {
    ExprResult Arg = DefaultLvalueConversion(Args[I]);
    if (Arg.isInvalid())
      return ExprError();
    Args[I] = Arg.get();
  }

  return new (Context) CXXReflectionTraitExpr(Context, Ty, Query, Args, 
                                              KWLoc, LParenLoc, RParenLoc);
}

ExprResult Sema::ActOnCXXUnreflexprExpression(SourceLocation Loc,
                                              Expr *Reflection) {
  return BuildCXXUnreflexprExpression(Loc, Reflection);
}

ExprResult Sema::BuildCXXUnreflexprExpression(SourceLocation Loc,
                                              Expr *E) {
  // TODO: Process the reflection E, UnresolveLookupExpr

  // Don't act on dependent expressions, just preserve them.
  // if (E->isTypeDependent() || E->isValueDependent())
  //   return new (Context) CXXUnreflexprExpr(E, Context.DependentTy,
  //                                          VK_RValue, OK_Ordinary, Loc);

  // The operand must be a reflection.
  // if (!CheckReflectionOperand(*this, E))
  //   return ExprError();

  // TODO: Process the reflection E, into DeclRefExpr

  // CXXUnreflexprExpr *Val =
  //     new (Context) CXXUnreflexprExpr(E, E->getType(), E->getValueKind(),
  //                                     E->getObjectKind(), Loc);

  // return Val;
  return ExprError();
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
  // if (!CheckReflectionOperand(*this, E))
  //   return QualType();

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

  Reflection R(Context, Result.Val);
  if (R.isType()) {
    Diag(E->getExprLoc(), diag::err_expression_not_type_reflection);
    return QualType();
  }

  // Get the type of the reflected entity.
  QualType Reflected = R.getAsType();

  return Context.getReflectedType(E, Reflected);
}
