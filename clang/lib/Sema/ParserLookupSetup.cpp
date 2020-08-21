//===--- ParserLookupSetup.cpp - Declaration Specifier Semantic Analysis -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file implements semantic analysis for declaration specifiers.
//
//===----------------------------------------------------------------------===//

#include "clang/Lex/Preprocessor.h"
#include "clang/Sema/ParserLookupSetup.h"
#include "clang/Sema/SemaInternal.h"

namespace clang {
namespace sema {

ParserLookupSetup::ParserLookupSetup(Sema &SemaRef, DeclContext *CurContext)
  : SemaRef(SemaRef) {
  VisitParentsInOrder(CurContext);
  MergeWithSemaState();
}

ParserLookupSetup::~ParserLookupSetup() {
  std::swap(SemaRef.IdResolver, IdResolver);
  // delete IdResolver;

  while (CurScope) {
    Scope *ParentScope = CurScope->getParent();
    delete CurScope;
    CurScope = ParentScope;
  }
}

void ParserLookupSetup::AddScope(unsigned ScopeFlags) {
  CurScope = new Scope(CurScope, ScopeFlags, SemaRef.PP.getDiagnostics());
}

void ParserLookupSetup::AddDecl(Decl *D) {
  if (NamedDecl *ND = castAsResolveableDecl(D)) {
    CurScope->AddDecl(ND);
    IdResolver->AddDecl(ND);
  }
}

void ParserLookupSetup::MergeWithSemaState() {
  for (Scope *Scope : SemaRef.getTTParseScopes()) {
    AddScope(Scope->getFlags());
  }
}

void ParserLookupSetup::VisitTranslationUnitDecl(TranslationUnitDecl *TUD) {
  AddScope(Scope::DeclScope);
}

void ParserLookupSetup::VisitNamespaceDecl(NamespaceDecl *NSD) {
  AddScope(Scope::DeclScope);
}

void ParserLookupSetup::VisitCXXRecordDecl(CXXRecordDecl *RD) {
  AddScope(Scope::ClassScope | Scope::DeclScope);
}

void ParserLookupSetup::VisitFunctionDecl(FunctionDecl *FD) {
  AddScope(Scope::FnScope | Scope::DeclScope | Scope::CompoundStmtScope);
}

}} // end namespace clang::sema
