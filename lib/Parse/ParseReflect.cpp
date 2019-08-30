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
  Sema::CXXReflectionScopeRAII ReflectionScope(Actions);

  // The operand is unevaluated.
  EnterExpressionEvaluationContext Unevaluated(
      Actions, Sema::ExpressionEvaluationContext::Unevaluated);

  // Perform the tentative parses first since isCXXTypeId tends to rewrite
  // tokens, which can subsequent parses a bit wonky.

  // Tentatively parse a template-name.
  {
    TentativeParsingAction TPA(*this);
    ParsedTemplateArgument T = ParseTemplateTemplateArgument();
    if (!T.isInvalid()) {
      TPA.Commit();
      return Actions.ActOnReflectedTemplate(T);
    }
    TPA.Revert();
  }

  // Otherwise, check for the global namespace
  if (Tok.is(tok::coloncolon) && NextToken().is(tok::r_paren)) {
    SourceLocation ColonColonLoc = ConsumeToken();
    return Actions.ActOnReflectedNamespace(ColonColonLoc);
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

  // Otherwise, try parsing this as type-id.
  if (isCXXTypeId(TypeIdAsTemplateArgument)) {
    // FIXME: Create a new DeclaratorContext?
    TypeResult T =
      ParseTypeName(nullptr, DeclaratorContext::TemplateArgContext);
    if (T.isInvalid()) {
      return ParsedReflectionOperand();
    }
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
  if (PR.isInvalid()) {
    T.skipToEnd();
    return ExprError();
  }

  if (T.consumeClose())
    return ExprError();

  return Actions.ActOnCXXReflectExpr(KWLoc, PR,
                                     T.getOpenLocation(),
                                     T.getCloseLocation());
}

/// Parse an invalid reflection.
///
/// \verbatim
///  primary-expression:
///    __valid_reflection '(' error-message ')'
/// \endverbatim
ExprResult Parser::ParseCXXInvalidReflectionExpression() {
  assert(Tok.is(tok::kw___invalid_reflection) && "Not '__invalid_reflection'");

  SourceLocation BuiltinLoc = ConsumeToken();
  BalancedDelimiterTracker T(*this, tok::l_paren);

  if (T.expectAndConsume(diag::err_expected_lparen_after, "__invalid_reflection"))
    return ExprError();

  ExprResult MessageExpr = ParseConstantExpression();

  if (MessageExpr.isInvalid()) {
    SkipUntil(tok::r_paren, StopAtSemi);
    return ExprError();
  }

  if (T.consumeClose())
    return ExprError();

  return Actions.ActOnCXXInvalidReflectionExpr(MessageExpr.get(), BuiltinLoc,
                                               T.getCloseLocation());
}

template<int BaseArgCount>
static llvm::Optional<SmallVector<Expr *, BaseArgCount>>
ParseConstexprFunctionArgs(Parser &P, BalancedDelimiterTracker &ParenTracker) {
  SmallVector<Expr *, BaseArgCount> Args;

  do {
    ExprResult Expr = P.ParseConstantExpression();
    if (Expr.isInvalid()) {
      ParenTracker.skipToEnd();
      return { };
    }
    Args.push_back(Expr.get());
  } while (P.TryConsumeToken(tok::comma));

  return Args;
}

/// Parse a reflection trait.
///
/// \verbatim
///   primary-expression:
///     __reflect '(' expression-list ')'
/// \endverbatim
ExprResult Parser::ParseCXXReflectionReadQuery() {
  assert(Tok.is(tok::kw___reflect) && "Not __reflect");
  SourceLocation Loc = ConsumeToken();

  // Parse any number of arguments in parens.
  BalancedDelimiterTracker Parens(*this, tok::l_paren);
  if (Parens.expectAndConsume())
    return ExprError();

  auto Args = ParseConstexprFunctionArgs<2>(*this, Parens);
  if (!Args)
    return ExprError();

  if (Parens.consumeClose())
    return ExprError();

  SourceLocation LPLoc = Parens.getOpenLocation();
  SourceLocation RPLoc = Parens.getCloseLocation();
  return Actions.ActOnCXXReflectionReadQuery(Loc, *Args, LPLoc, RPLoc);
}

/// Parse a reflection modification.
///
/// \verbatim
///   primary-expression:
///     __reflect_mod '(' expression-list ')'
/// \endverbatim
ExprResult Parser::ParseCXXReflectionWriteQuery() {
  assert(Tok.is(tok::kw___reflect_mod) && "Not __reflect_mod");
  SourceLocation Loc = ConsumeToken();

  // Parse any number of arguments in parens.
  BalancedDelimiterTracker Parens(*this, tok::l_paren);
  if (Parens.expectAndConsume())
    return ExprError();

  auto Args = ParseConstexprFunctionArgs<3>(*this, Parens);
  if (!Args)
    return ExprError();

  if (Parens.consumeClose())
    return ExprError();

  SourceLocation LPLoc = Parens.getOpenLocation();
  SourceLocation RPLoc = Parens.getCloseLocation();
  return Actions.ActOnCXXReflectionWriteQuery(Loc, *Args, LPLoc, RPLoc);
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

  auto Args = ParseConstexprFunctionArgs<2>(*this, Parens);
  if (!Args)
    return ExprError();

  if (Parens.consumeClose())
    return ExprError();

  SourceLocation LPLoc = Parens.getOpenLocation();
  SourceLocation RPLoc = Parens.getCloseLocation();
  return Actions.ActOnCXXReflectPrintLiteral(Loc, *Args, LPLoc, RPLoc);
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

ExprResult Parser::ParseCXXCompilerErrorExpression() {
  assert(Tok.is(tok::kw___compiler_error) && "Not '__compiler_error'");

  SourceLocation BuiltinLoc = ConsumeToken();
  BalancedDelimiterTracker T(*this, tok::l_paren);

  if (T.expectAndConsume(diag::err_expected_lparen_after, "__compiler_error"))
    return ExprError();

  ExprResult MessageExpr = ParseConstantExpression();

  if (MessageExpr.isInvalid()) {
    SkipUntil(tok::r_paren, StopAtSemi);
    return ExprError();
  }

  if (T.consumeClose())
    return ExprError();

  return Actions.ActOnCXXCompilerErrorExpr(MessageExpr.get(), BuiltinLoc,
                                           T.getCloseLocation());
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
///   reflected-unqualid-id:
///     'unqaulid' '(' reflection ')'
///
/// Returns true if parsing or semantic analysis fail.
bool Parser::ParseCXXReflectedId(CXXScopeSpec &SS,
                                 SourceLocation TemplateKWLoc,
                                 UnqualifiedId &Result) {
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

  DeclarationNameInfo NameInfo;
  if (Actions.BuildReflectedIdName(KWLoc, Parts,
                                   T.getCloseLocation(), NameInfo))
    return true;

  TemplateNameKind TNK;
  TemplateTy Template;
  if (Actions.BuildInitialDeclnameId(KWLoc, SS, NameInfo.getName(),
                                     TemplateKWLoc, TNK, Template, Result))
    return true;

  // FIXME: This should also workn if the built decl name is non dependent.
  //
  // Prioritize template parsing, represents whether we should treat the next
  // '<' token as introducing a template argument list, or a less than operator.
  bool PrioritizeTemplateParsing = TNK || !TemplateKWLoc.isInvalid();
  if (Tok.is(tok::less) && PrioritizeTemplateParsing) {
    SourceLocation LAngleLoc;
    TemplateArgList TemplateArgs;
    SourceLocation RAngleLoc;

    if (ParseTemplateIdAfterTemplateName(/*ConsumeLastToken=*/true,
                                         LAngleLoc, TemplateArgs, RAngleLoc))
      return true;

    ASTTemplateArgsPtr TemplateArgsPtr(TemplateArgs);
    return Actions.CompleteDeclnameId(KWLoc, SS, NameInfo.getName(),
                                      TemplateKWLoc, TNK, Template, LAngleLoc,
                                      TemplateArgsPtr, RAngleLoc, TemplateIds,
                                      Result, RAngleLoc);
  }

  return Actions.CompleteDeclnameId(KWLoc, SS, NameInfo.getName(), TemplateKWLoc,
                                    TNK, Template,
                                    /*LAngleLoc=*/SourceLocation(),
                                    /*TemplateArgsPtr=*/ASTTemplateArgsPtr(),
                                    /*RAngleLoc=*/SourceLocation(),
                                    TemplateIds, Result, T.getCloseLocation());
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

/// Parse a concatenation expression.
///
///   primary-expression:
///      '__concatenate' '(' constant-argument-list ')'
///
/// Each argument in the constant-argument-list must be a constant expression.
///
/// Returns true if parsing or semantic analysis fail.
ExprResult Parser::ParseCXXConcatenateExpression() {
  assert(Tok.is(tok::kw___concatenate));
  SourceLocation KeyLoc = ConsumeToken();

  BalancedDelimiterTracker Parens(*this, tok::l_paren);
  if (Parens.expectAndConsume())
    return ExprError();

  SmallVector<Expr *, 4> Parts;
  do {
    ExprResult Expr = ParseConditionalExpression();
    if (Expr.isInvalid()) {
      Parens.skipToEnd();
      return ExprError();
    }
    Parts.push_back(Expr.get());
  } while (TryConsumeToken(tok::comma));

  if (Parens.consumeClose())
    return ExprError();

  return Actions.ActOnCXXConcatenateExpr(Parts, KeyLoc,
                                         Parens.getOpenLocation(),
                                         Parens.getCloseLocation());
}

/// Returns true if reflection is enabled and the
/// current expression appears to be a variadic reifier.
bool Parser::isVariadicReifier() const {
  if (tok::isAnnotation(Tok.getKind()) || Tok.is(tok::raw_identifier))
     return false;
  IdentifierInfo *TokII = Tok.getIdentifierInfo();
  // If Reflection is enabled, the current token is a
  // a reifier keyword, followed by an open parentheses,
  // followed by an ellipsis, this is a variadic reifier.
  return getLangOpts().Reflection && TokII &&
    TokII->isReifierKeyword(getLangOpts())
    && PP.LookAhead(0).getKind() == tok::l_paren
    && PP.LookAhead(1).getKind() == tok::ellipsis;
}

bool Parser::ParseVariadicReifier(llvm::SmallVectorImpl<Expr *> &Exprs) {
  IdentifierInfo *KW = Tok.getIdentifierInfo();
  SourceLocation KWLoc = ConsumeToken();
  // Parse any number of arguments in parens.
  BalancedDelimiterTracker Parens(*this, tok::l_paren);
  if (Parens.expectAndConsume())
    return false;

  SourceLocation EllipsisLoc;
  TryConsumeToken(tok::ellipsis, EllipsisLoc);

  // FIXME: differentiate this return from an error, as
  // returning here means we have a non-variadic reifier.
  if (!EllipsisLoc.isValid())
    return false;

  ExprResult ReflRange = ParseConstantExpression();

  if (ReflRange.isInvalid()) {
    // TODO: Diag << KWLoc, err_invalid_reflection in parse
    return true;
  }

  if (ReflRange.isInvalid()) {
    Parens.skipToEnd();
    return true;
  }

  if (Parens.consumeClose())
    return true;

  SourceLocation LPLoc = Parens.getOpenLocation();
  SourceLocation RPLoc = Parens.getCloseLocation();;
  return Actions.ActOnVariadicReifier(Exprs, KWLoc, KW, ReflRange.get(),
                                      LPLoc, EllipsisLoc, RPLoc);
}

bool Parser::ParseVariadicReifier(llvm::SmallVectorImpl<QualType> &Types) {
  SourceLocation KWLoc = ConsumeToken();
  // Parse any number of arguments in parens.
  BalancedDelimiterTracker Parens(*this, tok::l_paren);
  if (Parens.expectAndConsume())
    return false;

  SourceLocation EllipsisLoc;
  TryConsumeToken(tok::ellipsis, EllipsisLoc);

  ExprResult ReflRange = ParseConstantExpression();

  if (ReflRange.isInvalid()) {
    Parens.skipToEnd();
    return true;
  }

  if (Parens.consumeClose())
    return true;

  SourceLocation LPLoc = Parens.getOpenLocation();
  SourceLocation RPLoc = Parens.getCloseLocation();
  return Actions.ActOnVariadicReifier(Types, KWLoc, ReflRange.get(),
                                      LPLoc, EllipsisLoc, RPLoc);
}
