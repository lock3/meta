//===--- Parser.cpp - C Language Family Parser ----------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements parsing for reflection facilities.
//
//===----------------------------------------------------------------------===//

#include "clang/Parse/Parser.h"
#include "clang/AST/ASTContext.h"
#include "clang/Parse/ParseDiagnostic.h"
#include "clang/Parse/RAIIObjectsForParser.h"
#include "clang/Sema/ParsedReflection.h"
using namespace clang;

/// Parse the operand of a reflexpr expression. This is almost exactly like
/// parsing a template argument, except that we also allow namespace-names
/// in this context.
ParsedReflectionOperand Parser::ParseCXXReflectOperand() {
  ParseScope ReflectionScope(this, Scope::ReflectionScope);

  // The operand is unevaluated.
  EnterExpressionEvaluationContext Unevaluated(
      Actions, Sema::ExpressionEvaluationContext::Unevaluated);

  // Perform the tentative parses first since isCXXTypeId tends to rewrite
  // tokens, which can subsequent parses a bit wonky.

  // Otherwise, tentatively parse a template-name.
  {
    TentativeParsingAction TPA(*this);
    ParsedTemplateArgument T = ParseTemplateTemplateArgument();
    if (!T.isInvalid()) {
      TPA.Commit();
      return Actions.ActOnReflectedTemplate(T);
    }
    TPA.Revert();
  }

  // Otherwise, tentatively parse a namespace-name.
  {
    TentativeParsingAction TPA(*this);
    CXXScopeSpec SS;
    SourceLocation IdLoc;
    Decl *D = ParseNamespaceName(SS, IdLoc);
    if (D) {
      TPA.Commit();
      return Actions.ActOnReflectedNamespace(SS, IdLoc, D);
    }
    TPA.Revert();
  }

  // Try parsing this as type-id first.
  if (isCXXTypeId(TypeIdAsTemplateArgument)) {
    // FIXME: Create a new DeclaratorContext?
    TypeResult T =
      ParseTypeName(nullptr, DeclaratorContext::TemplateArgContext);
    return Actions.ActOnReflectedType(T.get());
  }

  // Parse an expression. template argument.
  ExprResult E = ParseExpression(NotTypeCast);
  if (E.isInvalid() || !E.get())
    return ParsedReflectionOperand();

  return Actions.ActOnReflectedExpression(E.get());
}


/// Parse a reflect-expression.
///
/// \verbatim
///       reflect-expression:
///         'reflexpr' '(' type-id ')'
///         'reflexpr' '(' template-name ')'
///         'reflexpr' '(' namespace-name ')'
///         'reflexpr' '(' id-expression ')'
/// \endverbatim
ExprResult Parser::ParseCXXReflectExpression() {
  assert(Tok.is(tok::kw_reflexpr) && "expected 'reflexpr'");
  SourceLocation KWLoc = ConsumeToken();

  BalancedDelimiterTracker T(*this, tok::l_paren);
  if (T.expectAndConsume(diag::err_expected_lparen_after, "reflexpr"))
    return ExprError();

  ParsedReflectionOperand PR = ParseCXXReflectOperand();
  if (PR.isInvalid())
    return ExprError();

  if (T.consumeClose())
    return ExprError();

  return Actions.ActOnCXXReflectExpr(KWLoc, PR,
                                     T.getOpenLocation(),
                                     T.getCloseLocation());
}

/// Parse a reflection trait.
///
/// \verbatim
///   primary-expression:
///     __reflect '(' expression-list ')'
/// \endverbatim
ExprResult Parser::ParseCXXReflectionTrait() {
  assert(Tok.is(tok::kw___reflect) && "Not __reflect");
  SourceLocation Loc = ConsumeToken();

  // Parse any number of arguments in parens.
  BalancedDelimiterTracker Parens(*this, tok::l_paren);
  if (Parens.expectAndConsume())
    return ExprError();

  SmallVector<Expr *, 2> Args;
  do {
    ExprResult Expr = ParseConstantExpression();
    if (Expr.isInvalid()) {
      Parens.skipToEnd();
      return ExprError();
    }
    Args.push_back(Expr.get());
  } while (TryConsumeToken(tok::comma));

  if (Parens.consumeClose())
    return ExprError();

  SourceLocation LPLoc = Parens.getOpenLocation();
  SourceLocation RPLoc = Parens.getCloseLocation();
  return Actions.ActOnCXXReflectionTrait(Loc, Args, LPLoc, RPLoc);
}

/// Parse a reflective pretty print of integer and string values.
///
/// \verbatim
///   primary-expression:
///     __reflect_print '(' expression-list ')'
/// \endverbatim
ExprResult Parser::ParseCXXReflectPrintLiteralExpression() {
  assert(Tok.is(tok::kw___reflect_print) && "Not __reflect_print");
  SourceLocation Loc = ConsumeToken();

  // Parse any number of arguments in parens.
  BalancedDelimiterTracker Parens(*this, tok::l_paren);
  if (Parens.expectAndConsume())
    return ExprError();

  SmallVector<Expr *, 2> Args;
  do {
    ExprResult Expr = ParseConstantExpression();
    if (Expr.isInvalid()) {
      Parens.skipToEnd();
      return ExprError();
    }
    Args.push_back(Expr.get());
  } while (TryConsumeToken(tok::comma));

  if (Parens.consumeClose())
    return ExprError();

  SourceLocation LPLoc = Parens.getOpenLocation();
  SourceLocation RPLoc = Parens.getCloseLocation();
  return Actions.ActOnCXXReflectPrintLiteral(Loc, Args, LPLoc, RPLoc);
}

/// Parse a reflective pretty print of a reflection.
///
/// \verbatim
///   primary-expression:
///     __reflect_pretty_print '(' reflection ')'
/// \endverbatim
ExprResult Parser::ParseCXXReflectPrintReflectionExpression() {
  assert(Tok.is(tok::kw___reflect_pretty_print) && "Not __reflect_pretty_print");
  SourceLocation Loc = ConsumeToken();

  // Parse any number of arguments in parens.
  BalancedDelimiterTracker Parens(*this, tok::l_paren);
  if (Parens.expectAndConsume())
    return ExprError();

  ExprResult Reflection = ParseConstantExpression();
  if (Reflection.isInvalid()) {
    Parens.skipToEnd();
    return ExprError();
  }

  if (Parens.consumeClose())
    return ExprError();

  SourceLocation LPLoc = Parens.getOpenLocation();
  SourceLocation RPLoc = Parens.getCloseLocation();
  return Actions.ActOnCXXReflectPrintReflection(Loc, Reflection.get(),
                                                LPLoc, RPLoc);
}

/// Parse a reflective dump of a reflection.
///
/// \verbatim
///   primary-expression:
///     __reflect_dump '(' reflection ')'
/// \endverbatim
ExprResult Parser::ParseCXXReflectDumpReflectionExpression() {
  assert(Tok.is(tok::kw___reflect_dump) && "Not __reflect_dump");
  SourceLocation Loc = ConsumeToken();

  // Parse any number of arguments in parens.
  BalancedDelimiterTracker Parens(*this, tok::l_paren);
  if (Parens.expectAndConsume())
    return ExprError();

  ExprResult Reflection = ParseConstantExpression();
  if (Reflection.isInvalid()) {
    Parens.skipToEnd();
    return ExprError();
  }

  if (Parens.consumeClose())
    return ExprError();

  SourceLocation LPLoc = Parens.getOpenLocation();
  SourceLocation RPLoc = Parens.getCloseLocation();
  return Actions.ActOnCXXReflectDumpReflection(Loc, Reflection.get(),
                                               LPLoc, RPLoc);
}

/// Parse an idexpr expression.
///
/// \verbatim
///   primary-expression:
///     idexpr '(' constant-expression ')'
/// \endverbatim
ExprResult Parser::ParseCXXIdExprExpression() {
  assert(Tok.is(tok::kw_idexpr) && "Not idexpr");
  SourceLocation Loc = ConsumeToken();

  // Parse any number of arguments in parens.
  BalancedDelimiterTracker Parens(*this, tok::l_paren);
  if (Parens.expectAndConsume())
    return ExprError();

  ExprResult Expr = ParseConstantExpression();
  if (Expr.isInvalid()) {
    Parens.skipToEnd();
    return ExprError();
  }

  if (Parens.consumeClose())
    return ExprError();

  SourceLocation LPLoc = Parens.getOpenLocation();
  SourceLocation RPLoc = Parens.getCloseLocation();
  return Actions.ActOnCXXIdExprExpr(Loc, Expr.get(), LPLoc, RPLoc);
}

/// Parse a valueof expression.
///
/// \verbatim
///   primary-expression:
///     valueof '(' constant-expression ')'
/// \endverbatim
ExprResult Parser::ParseCXXValueOfExpression() {
  assert(Tok.is(tok::kw_valueof) && "Not valueof");
  SourceLocation Loc = ConsumeToken();

  // Parse any number of arguments in parens.
  BalancedDelimiterTracker Parens(*this, tok::l_paren);
  if (Parens.expectAndConsume())
    return ExprError();

  ExprResult Expr = ParseConstantExpression();
  if (Expr.isInvalid()) {
    Parens.skipToEnd();
    return ExprError();
  }

  if (Parens.consumeClose())
    return ExprError();

  SourceLocation LPLoc = Parens.getOpenLocation();
  SourceLocation RPLoc = Parens.getCloseLocation();
  return Actions.ActOnCXXValueOfExpr(Loc, Expr.get(), LPLoc, RPLoc);
}


/// Parse a reflected id
///
///   unqualified-id:
///     'unqaulid' '(' reflection ')'
///
/// Returns true if parsing or semantic analysis fail.
bool Parser::ParseCXXReflectedId(UnqualifiedId& Result) {
  assert(Tok.is(tok::kw_unqualid) && "expected 'unqualid'");
  SourceLocation KWLoc = ConsumeToken();

  BalancedDelimiterTracker T(*this, tok::l_paren);
  if (T.expectAndConsume(diag::err_expected_lparen_after, "unqualid"))
    return true;

  SmallVector<Expr *, 4> Parts;
  while (true) {
    ExprResult Result = ParseConstantExpression();
    if (Result.isInvalid())
      return true;
    Parts.push_back(Result.get());
    if (Tok.is(tok::r_paren))
      break;
    if (ExpectAndConsume(tok::comma))
      return true;
  }
  if (T.consumeClose())
    return true;

  return Actions.BuildDeclnameId(Parts, Result, KWLoc,
                                 T.getCloseLocation());
}

/// Parse a reflected-value-expression.
///
/// \verbatim
///   unreflexpr-expression:
///     'unreflexpr' '(' reflection ')'
/// \endverbatim
///
/// The constant expression must be a reflection of a type.
ExprResult Parser::ParseCXXUnreflexprExpression() {
  assert(Tok.is(tok::kw_unreflexpr) && "expected 'unreflexpr'");
  SourceLocation Loc = ConsumeToken();

  BalancedDelimiterTracker T(*this, tok::l_paren);
  if (T.expectAndConsume(diag::err_expected_lparen_after, "unreflexpr"))
    return ExprError();
  ExprResult Result = ParseConstantExpression();
  if (T.consumeClose())
    return ExprError();
  if (Result.isInvalid())
    return ExprError();
  return Actions.ActOnCXXUnreflexprExpression(Loc, Result.get());
}

/// Parse a type reflection specifier.
///
/// \verbatim
///   reflection-type-specifier:
///     'typename' '(' reflection ')'
/// \endverbatim
///
/// The constant expression must be a reflection of a type.
TypeResult Parser::ParseReflectedTypeSpecifier(SourceLocation TypenameLoc,
                                               SourceLocation &EndLoc) {
  BalancedDelimiterTracker T(*this, tok::l_paren);
  if (T.expectAndConsume(diag::err_expected_lparen_after, "reflexpr"))
    return TypeResult(true);
  ExprResult Result = ParseConstantExpression();
  if (!T.consumeClose()) {
    EndLoc = T.getCloseLocation();
    if (!Result.isInvalid())
      return Actions.ActOnReflectedTypeSpecifier(TypenameLoc, Result.get());
  }
  return TypeResult(true);
}

/// Parse a template argument reflection.
///
/// \verbatim
///   reflection-template-argument:
///     'templarg' '(' reflection ')'
/// \endverbatim
///
/// The constant expression must be a reflection of a type.
ParsedTemplateArgument
Parser::ParseReflectedTemplateArgument() {
  assert(Tok.is(tok::kw_templarg) && "expected 'templarg'");
  SourceLocation Loc = ConsumeToken();

  BalancedDelimiterTracker T(*this, tok::l_paren);
  if (T.expectAndConsume(diag::err_expected_lparen_after, "templarg"))
    return ParsedTemplateArgument();
  ExprResult Result = ParseConstantExpression();
  if (T.consumeClose())
    return ParsedTemplateArgument();
  if (Result.isInvalid())
    return ParsedTemplateArgument();

  return Actions.ActOnReflectedTemplateArgument(Loc, Result.get());
}
