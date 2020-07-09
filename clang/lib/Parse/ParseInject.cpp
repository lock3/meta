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
#include "clang/AST/PrettyDeclStackTrace.h"
#include "clang/Parse/ParseDiagnostic.h"
#include "clang/Parse/Parser.h"
#include "clang/Parse/RAIIObjectsForParser.h"

using namespace clang;

/// ParseCXXNamespaceFragment
Decl *Parser::ParseCXXNamespaceFragment(Decl *Fragment) {
  assert(Tok.is(tok::kw_namespace) && "expected 'namespace'");

  SourceLocation NamespaceLoc = ConsumeToken();
  IdentifierInfo *Id = nullptr;
  SourceLocation IdLoc;
  if (isIdentifier()) {
    Id = Tok.getIdentifierInfo();
    IdLoc = ConsumeIdentifier();
  }

  BalancedDelimiterTracker T(*this, tok::l_brace);
  if (T.consumeOpen()) {
    Diag(Tok, diag::err_expected) << "namespace-fragment";
    Actions.ActOnFinishCXXFragment(getCurScope(), nullptr, nullptr);
    return nullptr;
  }

  ParseScope NamespaceScope(this, Scope::DeclScope);

  SourceLocation InlineLoc;
  ParsedAttributesWithRange Attrs(AttrFactory);
  UsingDirectiveDecl *ImplicitUsing = nullptr;
  Decl *Ns = Actions.ActOnStartNamespaceDef(getCurScope(), InlineLoc,
                                            NamespaceLoc, IdLoc, Id,
                                            T.getOpenLocation(),
                                            Attrs, ImplicitUsing);

  // Parse the declarations within the namespace. Note that this will match
  // the closing brace. We don't allow nested specifiers for the vector.
  InnerNamespaceInfoList InnerNSs;
  ParseInnerNamespace(InnerNSs, 0, InlineLoc, Attrs, T);

  NamespaceScope.Exit();

  Actions.ActOnFinishNamespaceDef(Ns, T.getCloseLocation());
  if (Ns->isInvalidDecl())
    return nullptr;

  return Actions.ActOnFinishCXXFragment(getCurScope(), Fragment, Ns);
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
  if (!isIdentifier() && Tok.isNot(tok::l_brace)) {
    Diag(Tok, diag::err_expected) << "class-fragment";
    Actions.ActOnFinishCXXFragment(getCurScope(), nullptr, nullptr);
    return nullptr;
  }

  IdentifierInfo *Id = nullptr;
  SourceLocation IdLoc;
  if (isIdentifier()) {
    Id = Tok.getIdentifierInfo();
    IdLoc = ConsumeIdentifier();
  }

  // Build a tag type for the injected class.
  CXXScopeSpec SS;
  MultiTemplateParamsArg MTP;
  bool IsOwned;
  bool IsDependent;
  TypeResult UnderlyingType;
  Decl *ClassDecl = Actions.ActOnTag(getCurScope(), TagType,
                                     /*Metafunction=*/nullptr,
                                     Sema::TUK_Definition, ClassKeyLoc, SS,
                                     Id, IdLoc, ParsedAttributesView(),
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

/// ParseCXXEnumFragment
Decl *Parser::ParseCXXEnumFragment(Decl *Fragment) {
  assert(Tok.is(tok::kw_enum) && "expected 'enum'");

  SourceLocation EnumKWLoc = ConsumeToken();

  // FIXME: We could accept an idexpr here, except that those names aren't
  // exported. They're really only meant to be used for self-references
  // within the fragment.
  if (!isIdentifier() && Tok.isNot(tok::l_brace)) {
    Diag(Tok, diag::err_expected) << "enum-fragment";
    Actions.ActOnFinishCXXFragment(getCurScope(), nullptr, nullptr);
    return nullptr;
  }

  IdentifierInfo *Id = nullptr;
  SourceLocation IdLoc;
  if (isIdentifier()) {
    Id = Tok.getIdentifierInfo();
    IdLoc = ConsumeIdentifier();
  }

  // Build a tag type for the injected class.
  CXXScopeSpec SS;
  MultiTemplateParamsArg MTP;
  bool IsOwned;
  bool IsDependent;
  TypeResult UnderlyingType;
  Decl *EnumDecl = Actions.ActOnTag(getCurScope(), DeclSpec::TST_enum,
                                    /*Metafunction=*/nullptr,
                                    Sema::TUK_Definition, EnumKWLoc, SS,
                                    Id, IdLoc, ParsedAttributesView(),
                                    /*AccessSpecifier=*/AS_none,
                                    /*ModulePrivateLoc=*/SourceLocation(),
                                    MTP, IsOwned, IsDependent,
                                    /*ScopedEnumKWLoc=*/SourceLocation(),
                                    /*ScopeEnumUsesClassTag=*/false,
                                    UnderlyingType,
                                    /*IsTypeSpecifier=*/false,
                                    /*IsTemplateParamOrArg=*/false,
                                    /*SkipBody=*/nullptr);

  // Parse the enum definition.
  ParseEnumBody(EnumKWLoc, EnumDecl);

  return Actions.ActOnFinishCXXFragment(getCurScope(), Fragment, EnumDecl);
}

Decl *Parser::ParseCXXStmtFragment(SourceLocation IntroLoc, bool HasThisPtr) {
  Sema::CXXThisScopeRAII ThisOverride(Actions, HasThisPtr);

  Scope *S = getCurScope();
  Decl *StmtFragment = Actions.ActOnStartCXXStmtFragment(S, IntroLoc,
                                                         HasThisPtr);
  // Parse the actual block. This consumes the braces.
  StmtResult Block = ParseCompoundStatementBody();
  if (Block.isInvalid()) {
    Actions.ActOnCXXStmtFragmentError(S);
    return nullptr;
  }

  return Actions.ActOnFinishCXXStmtFragment(S, StmtFragment, Block.get());
}

/// ParseCXXBlockFragment
Decl *Parser::ParseCXXBlockFragment(Decl *Fragment) {
  assert(Tok.is(tok::l_brace) && "Expected '{'");
  SourceLocation IntroLoc = Tok.getLocation();

  Decl *StmtFragment = ParseCXXStmtFragment(IntroLoc, /*HasThisPtr=*/false);
  return Actions.ActOnFinishCXXFragment(getCurScope(), Fragment, StmtFragment);
}

/// ParseCXXMemberBlockFragment
Decl *Parser::ParseCXXMemberBlockFragment(Decl *Fragment) {
  assert(Tok.is(tok::kw_this) && "Expected 'this'");

  SourceLocation IntroLoc = Tok.getLocation();
  if (ExpectAndConsume(tok::kw_this))
    return nullptr;


  Decl *StmtFragment = ParseCXXStmtFragment(IntroLoc, /*HasThisPtr=*/true);
  return Actions.ActOnFinishCXXFragment(getCurScope(), Fragment, StmtFragment);
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
  // A name declared in the the fragment is not leaked into the enclosing
  // scope. That is, fragments names are only accessible from within.
  ParseScope FragmentScope(this, Scope::DeclScope | Scope::FragmentScope);

  // Start the fragment. The fragment is finished in one of the
  // ParseCXX*Fragment functions.
  Decl *Fragment = Actions.ActOnStartCXXFragment(
      getCurScope(), Tok.getLocation());
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

ExprResult Parser::ParseCXXUnquoteOperator() {
  assert(Tok.is(tok::percentl_brace) && "Expected '%{'");

  unsigned Offset = PendingUnquotes.size();
  CachedTokens &Toks = PendingUnquotes.emplace_back();

  SourceLocation BeginLoc = ConsumeBrace();
  ConsumeAndStoreUntil(tok::r_brace, Toks,
      /*StopAtSemi=*/false, /*ConsumeFinalToken=*/false);
  SourceLocation EndLoc = ConsumeBrace();

  return Actions.ActOnCXXFragmentCaptureExpr(BeginLoc, Offset, EndLoc);
}

bool Parser::ParseCXXFragmentCaptures(SmallVectorImpl<Expr *> &Captures) {
  Token CurrentToken = Tok;

  // FIXME: Is there a better way to replay these tokens?
  for (CachedTokens Tokens : PendingUnquotes) {
    // Enqueue the token that was the "current token" prior to
    // entering ParseCXXFragmentCaptures so that it's the token we end
    // with.
    Tokens.push_back(CurrentToken);

    // Re-enter the stored parenthesized tokens into the token stream, so we may
    // parse them now.
    PP.EnterTokenStream(Tokens, /*DisableMacroExpansion*/ true,
                        /*IsReinject*/ true);

    // Drop the current token and bring the first cached one. It's the same token
    // as when we entered this function.
    ConsumeAnyToken();

    ExprResult Result = ParseConstantExpression();
    if (Result.isInvalid()) {
      PendingUnquotes.clear();
      return true;
    }

    Captures.push_back(Result.get());
  }

  PendingUnquotes.clear();
  return false;
}

/// ParseCXXFragmentExpression
///
///       fragment-expression:
///         'fragment' fragment
///
ExprResult Parser::ParseCXXFragmentExpression() {
  assert(Tok.is(tok::kw_fragment) && "expected 'fragment' token");
  SourceLocation Loc = ConsumeToken();

  // Enter a new fragment parse context.
  CXXFragmentParseRAII ParseStateRAII(*this);

  Decl *Fragment = ParseCXXFragment();
  if (!Fragment)
    return ExprError();

  // Parse the capture stack.
  SmallVector<Expr *, 10> Captures;
  if (ParseCXXFragmentCaptures(Captures))
    return ExprError();

  return Actions.ActOnCXXFragmentExpr(Loc, Fragment, Captures);
}

Decl *Parser::ParseNamespaceDeclForInjectionContext() {
  // Check for the global namespace
  if (Tok.is(tok::coloncolon) && NextToken().is(tok::r_paren)) {
    ConsumeToken(); // Eat `::`
    return Actions.Context.getTranslationUnitDecl();
  }

  CXXScopeSpec SS;
  SourceLocation IdLoc;
  return ParseNamespaceName(SS, IdLoc);
}

/// Parses an optional injection context specifier.
///
/// Returns true if invalid.
bool
Parser::ParseOptionalCXXInjectionContextSpecifier(
                                      CXXInjectionContextSpecifier &Specifier) {
  if (Tok.is(tok::kw_namespace)) {
    SourceLocation KWLocation = ConsumeToken();
    if (Tok.is(tok::l_paren)) {
      BalancedDelimiterTracker Parens(*this, tok::l_paren);
      if (Parens.expectAndConsume())
        return true;

      Decl *NamespaceDecl = ParseNamespaceDeclForInjectionContext();
      if (!NamespaceDecl) {
        Parens.skipToEnd();
        return true;
      }

      if (Parens.consumeClose())
        return true;

      return Actions.ActOnCXXSpecifiedNamespaceInjectionContext(
               KWLocation, NamespaceDecl, Specifier, Parens.getCloseLocation());
    } else {
      return Actions.ActOnCXXParentNamespaceInjectionContext(
                                                         KWLocation, Specifier);
    }
  }

  return false;
}

/// Parse a C++ injection statement.
///
///   injection-statement:
///     '->' fragment ';'
///     '->' reflection ';'
///
/// Note that the statement parser will collect the trailing semicolon.
StmtResult Parser::ParseCXXInjectionStatement() {
  assert(Tok.is(tok::arrow) && "expected '->' token");
  SourceLocation Loc = ConsumeToken();

  CXXInjectionContextSpecifier ICS;
  if (ParseOptionalCXXInjectionContextSpecifier(ICS))
    return StmtError();

  /// Get a fragment or reflection as the operand of the injection statement.
  ExprResult Operand = ParseConstantExpression();
  if (Operand.isInvalid())
    return StmtError();

  Operand = Actions.CorrectDelayedTyposInExpr(Operand);
  if (Operand.isInvalid() || Operand.get()->containsErrors())
    return StmtError();

  return Actions.ActOnCXXInjectionStmt(Loc, ICS, Operand.get());
}

/// Parse a C++ injection declaration.
///
///   injection-declaration:
///     '__inject_base' '(' base-specifier-list ')' ';'
///
/// Returns the group of declarations parsed.

StmtResult Parser::ParseCXXBaseInjectionStatement() {
  assert(Tok.is(tok::kw___inject_base) && "expected '__inject_base' token");
  SourceLocation KWLoc = ConsumeToken();

  BalancedDelimiterTracker Parens(*this, tok::l_paren);
  if (Parens.expectAndConsume())
    return StmtError();

  auto BaseSpecifiers = ParseCXXBaseSpecifierList(/*ClassDecl=*/nullptr);
  if (BaseSpecifiers.empty())
    return StmtError();

  if (Parens.consumeClose())
    return StmtError();

  SourceLocation LPLoc = Parens.getOpenLocation();
  SourceLocation RPLoc = Parens.getCloseLocation();
  return Actions.ActOnCXXBaseInjectionStmt(KWLoc, LPLoc, BaseSpecifiers, RPLoc);
}

namespace {
  template<typename T>
  class MetaParseScope {
    Parser::ParseScope PS;

  public:
    MetaParseScope(Parser *Parser, Decl *MetaDecl)
      : PS(Parser, Scope::FnScope | Scope::DeclScope) {
      // Set our body scope's current entity to be the function
      // representation of our metaprogram if not set,
      // fragment capture does not work correctly.
      Parser->getCurScope()->setEntity(cast<T>(MetaDecl)->getFunctionDecl());
    }
  };
}

/// Parse an injector-declaration.
///
///  injector-declaration:
///    metaprogram-declaration
///    terminated-injection-declaration
///
Decl *Parser::MaybeParseCXXInjectorDeclaration() {
  assert(Tok.is(tok::kw_consteval));

  // [Meta] metaprogram-declaration
  if (NextToken().is(tok::l_brace))
    return ParseCXXMetaprogramDeclaration();

  // [Meta] terminated-injection-declaration
  if (NextToken().is(tok::arrow))
    return ParseCXXTerminatedInjectionDeclaration();

  return nullptr;
}

/// Parse a metaprogram-declaration.
///
/// \verbatim
///   metaprogram-declaration:
///     'consteval' compound-statement
/// \endverbatim
Decl *Parser::ParseCXXMetaprogramDeclaration() {
  assert(Tok.is(tok::kw_consteval));
  SourceLocation ConstevalLoc = ConsumeToken();
  assert(Tok.is(tok::l_brace));

  Decl *D = Actions.ActOnCXXMetaprogramDecl(ConstevalLoc);

  // Enter a scope for the metaprogram declaration body.
  MetaParseScope<CXXMetaprogramDecl> MetaScope(this, D);

  DeclContext *OriginalDC;
  Actions.ActOnStartCXXMetaprogramDecl(D, OriginalDC);

  PrettyDeclStackTraceEntry CrashInfo(Actions.getASTContext(), D, ConstevalLoc,
                                      "parsing metaprogram declaration body");

  // Parse the body of the metaprogram declaration.
  StmtResult Body(ParseCompoundStatementBody());
  if (!Body.isInvalid())
    Actions.ActOnFinishCXXMetaprogramDecl(D, Body.get(),
                                          OriginalDC, /*FromParser=*/true);
  else
    Actions.ActOnCXXMetaprogramDeclError(D, OriginalDC);

  return D;
}

Decl *Parser::ParseCXXInjectionDeclaration(bool IncludeTerminator) {
  assert(Tok.is(tok::kw_consteval));
  SourceLocation ConstevalLoc = ConsumeToken();
  assert(Tok.is(tok::arrow) && "expected '->' token");

  Decl *D = Actions.ActOnCXXInjectionDecl(ConstevalLoc);

  // Enter a scope for the constexpr declaration body.
  MetaParseScope<CXXInjectionDecl> MetaScope(this, D);

  DeclContext *OriginalDC;
  Actions.ActOnStartCXXInjectionDecl(D, OriginalDC);

  PrettyDeclStackTraceEntry CrashInfo(Actions.getASTContext(), D, ConstevalLoc,
                                      "parsing injection declaration body");

  // Parse the injection statement of the metaprogram declaration.
  StmtResult InjectionStmt = ParseCXXInjectionStatement();
  if (!InjectionStmt.isInvalid())
    Actions.ActOnFinishCXXInjectionDecl(D, InjectionStmt.get(),
                                        OriginalDC, /*FromParser=*/true);
  else
    Actions.ActOnCXXInjectionDeclError(D, OriginalDC);

  if (IncludeTerminator) {
    // Parse the ending simicolon.
    ExpectAndConsume(tok::semi);
  }

  return D;
}

/// Parse a C++ injection declaration.
///
///   injection-declaration:
///     'consteval' '->' fragment
///     'consteval' '->' reflection
///
/// Returns the injector declaration.
Decl *Parser::ParseCXXInjectionDeclaration() {
  return ParseCXXInjectionDeclaration(false);
}

/// Parse a C++ injection declaration.
///
///   terminated-injection-declaration:
///     injection-declaration ';'
///
/// Returns the injector declaration.
Decl *Parser::ParseCXXTerminatedInjectionDeclaration() {
  return ParseCXXInjectionDeclaration(true);
}

/// Parse a C++ injected parameter.
///
///   parameter:
///     '->' constant-expression
///
bool Parser::ParseCXXInjectedParameter(
                       SmallVectorImpl<DeclaratorChunk::ParamInfo> &ParamInfo) {
  assert(Tok.is(tok::arrow) && "Expected '->' token");
  SourceLocation Loc = ConsumeToken();

  ExprResult Reflection = ParseConstantExpression();
  if (Reflection.isInvalid())
    return false;

  Actions.ActOnCXXInjectedParameter(Loc, Reflection.get(), ParamInfo);
  return true;
}

/// Parse a generated type declaration.
///
///   type-transformer:
///     'using' class-key identifier 'as' type-generator
///
///   type-generator:
///     generator-name '(' reflection ')'
///
///   generator-name:
///     id-expression
///
/// FIXME: Support union as a class key? What about enum?
Parser::DeclGroupPtrTy
Parser::ParseCXXTypeTransformerDeclaration(SourceLocation UsingLoc) {
  assert(Tok.is(tok::kw_class) || Tok.is(tok::kw_struct));

  // Match the class key.
  bool IsClass = Tok.is(tok::kw_class);
  ConsumeToken();

  // Match the identifier.
  IdentifierInfo *Id = nullptr;
  SourceLocation IdLoc;
  if (isIdentifier()) {
    Id = Tok.getIdentifierInfo();
    IdLoc = ConsumeIdentifier();
  } else {
    Diag(Tok.getLocation(), diag::err_expected) << tok::identifier;
    return DeclGroupPtrTy();
  }

  // Match the context keyword "as".
  if (!isIdentifier()) {
    Diag(Tok.getLocation(), diag::err_expected) << "as";
    return DeclGroupPtrTy();
  }
  IdentifierInfo *As = Tok.getIdentifierInfo();
  if (As->getName() != "as") {
    Diag(Tok.getLocation(), diag::err_expected) << "as";
    return DeclGroupPtrTy();
  }
  ConsumeIdentifier();

  ExprResult Generator = ParseCXXIdExpression();
  if (Generator.isInvalid())
    return DeclGroupPtrTy();

  BalancedDelimiterTracker T(*this, tok::l_paren);
  if (T.expectAndConsume(diag::err_expected_lparen_after, "generator-name"))
    return DeclGroupPtrTy();
  ExprResult Reflection = ParseConstantExpression();
  if (Reflection.isInvalid())
    return DeclGroupPtrTy();
  if (T.consumeClose())
    return DeclGroupPtrTy();

  return Actions.ActOnCXXTypeTransformerDecl(UsingLoc, IsClass, IdLoc, Id,
                                             Generator.get(), Reflection.get());
}
