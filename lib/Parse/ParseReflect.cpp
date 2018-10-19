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
using namespace clang;


/// Parse a reflect-expression.
///
/// \verbatim
///       reflect-expression:
///         'reflexpr' '(' id-expression ')'
///         'reflexpr' '(' type-id ')'
///         'reflexpr' '(' namespace-name ')'
/// \endverbatim
ExprResult Parser::ParseCXXReflectExpression() {
  assert(Tok.is(tok::kw_reflexpr) && "expected 'reflexpr'");
  SourceLocation KWLoc = ConsumeToken();

  BalancedDelimiterTracker T(*this, tok::l_paren);
  if (T.expectAndConsume(diag::err_expected_lparen_after, "reflexpr"))
    return ExprError();

  unsigned Kind; // The reflected entity kind.
  ParsedReflectionPtr Entity; // The actual entity.

  // FIXME: The operand parsing is a bit fragile. We could do a lot better
  // by looking at tokens to characterize the parse before committing.
  //
  // Also, is it possible to reflect expressions within this framework?

  CXXScopeSpec SS;
  ParseOptionalCXXScopeSpecifier(SS, nullptr, /*EnteringContext=*/false);

  // If the next token is an identifier, try to resolve that. This will likely
  // match most uses of the reflection operator, but there are some cases
  // of id-expressions and type-ids that must be handled separately.
  //
  // FIXME: This probably won't work for things operator and conversion
  // functions.

  NestedNameSpecifier* NNS = SS.getScopeRep();
  if (!SS.isInvalid() && NNS && NNS->isDependent()) {
    IdentifierInfo *Id = Tok.getIdentifierInfo();
    SourceLocation IdLoc = ConsumeToken();
    if (!Actions.ActOnReflectedDependentId(SS, IdLoc, Id, Kind, Entity))
      return ExprError();
  } else if (!SS.isInvalid() && Tok.is(tok::identifier)) {
    IdentifierInfo *Id = Tok.getIdentifierInfo();
    SourceLocation IdLoc = ConsumeToken();

    if (!Actions.ActOnReflectedId(SS, IdLoc, Id, Kind, Entity))
      return ExprError();

  } else if (isCXXTypeId(TypeIdAsTemplateArgument)) {
    DeclSpec DS(AttrFactory);
    ParseSpecifierQualifierList(DS);
    Declarator D(DS, DeclaratorContext::TypeNameContext);
    ParseDeclarator(D);
    if (D.isInvalidType())
      return ExprError();
    if (!Actions.ActOnReflectedType(D, Kind, Entity))
      return ExprError();
  }
  // else if (!SS.isInvalid() && Tok.is(tok::annot_template_id)) {
  //   TemplateIdAnnotation *IdAnn = takeTemplateIdAnnotation(Tok);
    //   IdentifierInfo *Id = IdAnn->Name;
    //   SourceLocation IdLoc = ConsumeAnnotationToken();

    //   if(!Actions.ActOnReflectedDependentId(SS, IdLoc, Id, Kind, Entity))
    // 	return ExprError();
    // }

  if (T.consumeClose())
    return ExprError();
  
  return Actions.ActOnCXXReflectExpression(KWLoc, Kind, Entity, 
					   T.getOpenLocation(), 
					   T.getCloseLocation());
}

static ReflectionTrait ReflectionTraitKind(tok::TokenKind kind) {
  switch (kind) {
  default:
    llvm_unreachable("Not a known type trait");
#define REFLECTION_TRAIT_1(Spelling, K)                                        \
  case tok::kw_##Spelling:                                                     \
    return URT_##K;
#define REFLECTION_TRAIT_2(Spelling, K)                                        \
  case tok::kw_##Spelling:                                                     \
    return BRT_##K;
#include "clang/Basic/TokenKinds.def"
  }
}

static unsigned ReflectionTraitArity(tok::TokenKind kind) {
  switch (kind) {
  default:
    llvm_unreachable("Not a known type trait");
#define REFLECTION_TRAIT(N, Spelling, K)                                       \
  case tok::kw_##Spelling:                                                     \
    return N;
#include "clang/Basic/TokenKinds.def"
  }
}

/// Parse a reflection trait.
///
/// \verbatim
///   primary-expression:
///     unary-reflection-trait '(' expression ')'
///     binary-reflection-trait '(' expression ',' expression ')'
///
///   unary-reflection-trait:
///     '__reflect_index'
/// \endverbatim
ExprResult Parser::ParseCXXReflectionTrait() {
  tok::TokenKind Kind = Tok.getKind();
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
  SourceLocation RPLoc = Parens.getCloseLocation();

  // Make sure that the number of arguments matches the arity of trait.
  unsigned Arity = ReflectionTraitArity(Kind);
  if (Args.size() != Arity) {
    Diag(RPLoc, diag::err_type_trait_arity)
        << Arity << 0 << (Arity > 1) << (int)Args.size() << SourceRange(Loc);
    return ExprError();
  }

  ReflectionTrait Trait = ReflectionTraitKind(Kind);
  return Actions.ActOnCXXReflectionTrait(Loc, Trait, Args, RPLoc);
}

/// Parse a reflected id
///
///   unqualified-id:
///      '(.' reflection '.)'
///
/// Returns true if parsing or semantic analysis fail.
bool Parser::ParseCXXReflectedId(UnqualifiedId& Result) {
  assert(Tok.is(tok::l_paren_period));

  BalancedDelimiterTracker T(*this, tok::l_paren_period);
  if (T.expectAndConsume())
    return true;

  SmallVector<Expr *, 4> Parts;
  while (true) {
    ExprResult Result = ParseConstantExpression();
    if (Result.isInvalid())
      return true;
    Parts.push_back(Result.get());
    if (Tok.is(tok::period_r_paren))
      break;
    if (ExpectAndConsume(tok::comma))
      return true;
  }
  if (T.consumeClose())
    return true;

  return Actions.BuildDeclnameId(Parts, Result,
                                 T.getOpenLocation(), T.getCloseLocation());
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
  ExprResult Reflection = ParseConstantExpression();
  if (T.consumeClose())
    return ExprError();
  if (Reflection.isInvalid())
    return ExprError();
  return Actions.ActOnCXXUnreflexprExpression(Loc, Reflection.get());
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
