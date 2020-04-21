//===--- ParseLegacyFragment.cpp - Legacy Fragment Parsing ----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements parsing for legacy style, implicit capture C++
// fragments.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/PrettyDeclStackTrace.h"
#include "clang/Parse/ParseDiagnostic.h"
#include "clang/Parse/Parser.h"
#include "clang/Parse/RAIIObjectsForParser.h"

using namespace clang;

/// ParseCXXLegacyFragment
///
///      fragment-expression:
///        named-namespace-definition
///        class-specifier
///        enum-specifier
///        compound-statement
///
Decl *Parser::ParseCXXLegacyFragment(SmallVectorImpl<Expr *> &Captures) {
  // Implicitly capture automatic variables as captured constants.
  Actions.ActOnCXXLegacyFragmentCapture(Captures);

  // A name declared in the the fragment is not leaked into the enclosing
  // scope. That is, fragments names are only accessible from within.
  ParseScope FragmentScope(this, Scope::DeclScope | Scope::FragmentScope);

  // Start the fragment. The fragment is finished in one of the
  // ParseCXX*Fragment functions.
  Decl *Fragment = Actions.ActOnStartCXXLegacyFragment(
      getCurScope(), Tok.getLocation(), Captures);
  if (!Fragment)
    return nullptr;

  switch (Tok.getKind()) {
    case tok::kw_namespace:
      return ParseCXXNamespaceFragment(Fragment);

    case tok::kw_struct:
    case tok::kw_class:
    case tok::kw_union:
      return ParseCXXClassFragment(Fragment);

    case tok::kw_enum:
      return ParseCXXEnumFragment(Fragment);

    case tok::l_brace:
      return ParseCXXBlockFragment(Fragment);

    case tok::kw_this:
      return ParseCXXMemberBlockFragment(Fragment);

    default:
      break;
  }

  Actions.ActOnFinishCXXFragment(getCurScope(), nullptr, nullptr);
  Diag(Tok.getLocation(), diag::err_expected_fragment);
  SkipUntil(tok::semi, StopAtSemi | StopBeforeMatch);
  return nullptr;
}

/// ParseLegacyCXXFragmentExpression
///
///       legacy-fragment-expression:
///         '__fragment' fragment
///
ExprResult Parser::ParseCXXLegacyFragmentExpression() {
  assert(Tok.is(tok::kw___fragment) && "expected '__fragment' token");
  SourceLocation Loc = ConsumeToken();

  SmallVector<Expr *, 8> Captures;
  Decl *Fragment = ParseCXXLegacyFragment(Captures);
  if (!Fragment)
    return ExprError();

  return Actions.ActOnCXXLegacyFragmentExpr(Loc, Fragment, Captures);
}
