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
  : SemaRef(SemaRef),
    IdResolver(new (SemaRef.Context) IdentifierResolver(SemaRef.PP)) {
  VisitParentsInOrder(CurContext);
  MergeWithSemaState();

  // FIXME: This a built on a bad memory model, which we're forced
  // into by the identifier resolver
  std::swap(SemaRef.IdResolver, IdResolver);
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

    for (Decl *D : Scope->decls())
      AddDecl(D);
  }
}

void ParserLookupSetup::VisitTranslationUnitDecl(TranslationUnitDecl *TUD) {
  AddScope(Scope::DeclScope);

  for (Decl *D : TUD->decls())
    AddDecl(D);
}

void ParserLookupSetup::VisitNamespaceDecl(NamespaceDecl *NSD) {
  AddScope(Scope::DeclScope);

  for (Decl *D : NSD->decls())
    AddDecl(D);
}

void ParserLookupSetup::VisitCXXRecordDecl(CXXRecordDecl *RD) {
  AddScope(Scope::ClassScope | Scope::DeclScope);

  for (Decl *D : RD->decls())
    AddDecl(D);

  // Add the base class decls to the scope for this class.
  for (CXXBaseSpecifier Base : RD->bases()) {
    QualType &&BaseQt = Base.getTypeSourceInfo()->getType();
    CXXRecordDecl *BRD = BaseQt->getAsCXXRecordDecl();

    for (Decl *D : BRD->decls())
      AddDecl(D);
  }
}

void ParserLookupSetup::VisitFunctionDecl(FunctionDecl *FD) {
  AddScope(Scope::FnScope | Scope::DeclScope | Scope::CompoundStmtScope);

  for (unsigned P = 0, NumParams = FD->getNumParams(); P < NumParams; ++P) {
    ParmVarDecl *Param = FD->getParamDecl(P);

    if (Param->getIdentifier())
      AddDecl(Param);
  }
}

}} // end namespace clang::sema
