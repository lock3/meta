//=- ParserLookupSetup.h - Sema based lookup using parse scopes -*- C++ -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines ParserLookupSetup, a utility for performing unqualified
// lookup after parsing has completed.
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_SEMA_PARSERLOOKUPSETUP_H
#define LLVM_CLANG_SEMA_PARSERLOOKUPSETUP_H

#include "clang/AST/DeclVisitor.h"

namespace clang {

class IdentifierResolver;
class Scope;
class Sema;

namespace sema {

class ParserLookupSetup : public DeclVisitor<ParserLookupSetup> {
  Sema &SemaRef;

  IdentifierResolver *IdResolver;
  Scope *CurScope = nullptr;
public:
  ParserLookupSetup(Sema &SemaRef, DeclContext *CurContext);
  ~ParserLookupSetup();

  Scope *getCurScope() {
    return CurScope;
  }

  void AddScope(unsigned ScopeFlags);

  NamedDecl *castAsResolveableDecl(Decl *D) {
    // Verify Decl is a NamedDecl
    if (!isa<NamedDecl>(D))
      return nullptr;

    NamedDecl *ND = cast<NamedDecl>(D);
    // Verify the NamedDecl is one that can be found via lookup.
    if (!ND->getDeclName())
      return nullptr;

    if (isa<UsingDirectiveDecl>(ND))
      return nullptr;

    return ND;
  }

  void AddDecl(Decl *D);

  void VisitParentsInOrder(DeclContext *DC) {
    if (DeclContext *PCD = DC->getParent())
      VisitParentsInOrder(PCD);

    Visit(DC);
  }

  void MergeWithSemaState();

  void Visit(DeclContext *S) {
    DeclVisitor<ParserLookupSetup>::Visit(Decl::castFromDeclContext(S));
  }

  void VisitTranslationUnitDecl(TranslationUnitDecl *TUD);
  void VisitNamespaceDecl(NamespaceDecl *NSD);
  void VisitCXXRecordDecl(CXXRecordDecl *RD);
  void VisitFunctionDecl(FunctionDecl *FD);
};

}} // end namespace clang::sema

#endif
