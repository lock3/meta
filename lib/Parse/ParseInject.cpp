//===--- ParseInject.cpp - Reflection Parsing -----------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements parsing for C++ injection statements.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTConsumer.h"
#include "clang/Parse/ParseDiagnostic.h"
#include "clang/Parse/Parser.h"
#include "clang/Parse/RAIIObjectsForParser.h"

using namespace clang;

/// ParseCXXNamespaceFragment
Decl *Parser::ParseCXXNamespaceFragment(Decl *Fragment) {
  return nullptr;
}

/// ParseCXXClassFragment
Decl *Parser::ParseCXXClassFragment(Decl *Fragment) {
  assert(Tok.isOneOf(tok::kw_struct, tok::kw_class, tok::kw_union) &&
         "expected 'struct', 'class', or 'union'");

  DeclSpec::TST TagType;
  if (Tok.is(tok::kw_struct))
   TagType = DeclSpec::TST_struct;
  else if (Tok.is(tok::kw_class))
   TagType = DeclSpec::TST_class;
  else
   TagType = DeclSpec::TST_union;

  SourceLocation ClassKeyLoc = ConsumeToken();

  // FIXME: We could accept an idexpr here, except that those names aren't
  // exported. They're really only meant to be used for self-references
  // within the fragment.
  if (Tok.isNot(tok::identifier) && Tok.isNot(tok::l_brace)) {
    Diag(Tok.getLocation(), diag::err_expected) << "class-fragment";
    Actions.ActOnFinishCXXFragment(getCurScope(), nullptr, nullptr);
    return nullptr;
  }

  IdentifierInfo *Id = nullptr;
  SourceLocation IdLoc;
  if (Tok.is(tok::identifier)) {
    Id = Tok.getIdentifierInfo();
    IdLoc = ConsumeToken();
  }

  // Build a tag type for the injected class.
  CXXScopeSpec SS;
  MultiTemplateParamsArg MTP;
  bool IsOwned;
  bool IsDependent;
  TypeResult UnderlyingType;
  Decl *ClassDecl = Actions.ActOnTag(getCurScope(), TagType, Sema::TUK_Definition,
                                 ClassKeyLoc, SS,
                                 Id, IdLoc,
                                 ParsedAttributesView(),
                                 /*AccessSpecifier=*/AS_none,
                                 /*ModulePrivateLoc=*/SourceLocation(),
                                 MTP, IsOwned, IsDependent,
                                 /*ScopedEnumKWLoc=*/SourceLocation(),
                                 /*ScopeEnumUsesClassTag=*/false,
                                 UnderlyingType,
                                 /*IsTypeSpecifier=*/false,
                                 /*IsTemplateParamOrArg=*/false,
                                 /*SkipBody=*/nullptr);

  // Parse the class definition.
  ParsedAttributesWithRange PA(AttrFactory);
  ParseCXXMemberSpecification(ClassKeyLoc, SourceLocation(), PA, TagType,
                              ClassDecl);

  return Actions.ActOnFinishCXXFragment(getCurScope(), Fragment, ClassDecl);
}

/// ParseCXXFragment
///
///      fragment-expression:
///        named-namespace-definition
///        class-specifier
///        enum-specifier
///        compound-statement
///
Decl *Parser::ParseCXXFragment() {
  // Start the fragment. The fragment is finished in one of the
  // ParseCXX*Fragment functions.
  Decl *Fragment = Actions.ActOnStartCXXFragment(getCurScope(),
                                                 Tok.getLocation());

  switch (Tok.getKind()) {
    case tok::kw_namespace:
      return ParseCXXNamespaceFragment(Fragment);

    case tok::kw_struct:
    case tok::kw_class:
    case tok::kw_union:
      return ParseCXXClassFragment(Fragment);

    case tok::kw_enum:
      llvm_unreachable("enum fragments not implemented");

    case tok::l_brace:
      llvm_unreachable("block fragments not implemented");

    default:
      break;
  }

  Diag(Tok.getLocation(), diag::err_expected_fragment);
  SkipUntil(tok::semi);
  return nullptr;
}

/// ParseCXXFragmentExpression
///
///       fragment-expression:
///         '<<' fragment
///
ExprResult Parser::ParseCXXFragmentExpression() {
  assert(Tok.is(tok::kw___fragment) && "expected '<<' token");
  SourceLocation Loc = ConsumeToken();

  Decl *Fragment = ParseCXXFragment();
  if (!Fragment)
    return ExprError();

  return Actions.ActOnCXXFragmentExpr(Loc, Fragment);
}

