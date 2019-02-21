//=- AnalysisBasedWarnings.h - Sema warnings based on libAnalysis -*- C++ -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines AnalysisBasedWarnings, a worker object used by Sema
// that issues warnings based on dataflow-analysis.
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_SEMA_PARSERLOOKUPSETUP_H
#define LLVM_CLANG_SEMA_PARSERLOOKUPSETUP_H

#include "clang/AST/DeclVisitor.h"
#include "clang/Sema/SemaInternal.h"

namespace clang {

namespace sema {

class ParserLookupSetup : public DeclVisitor<ParserLookupSetup> {
  Sema &SemaRef;

  IdentifierResolver *IdResolver;
  Scope *CurScope = nullptr;
public:
  ParserLookupSetup(Sema &SemaRef, DeclContext *CurContext)
    : SemaRef(SemaRef),
      IdResolver(new (SemaRef.Context) IdentifierResolver(SemaRef.PP)) {
    VisitParentsInOrder(CurContext);
    MergeWithSemaState();

    // FIXME: This a built on a bad memory model, which we're forced
    // into by the identifier resolver
    std::swap(SemaRef.IdResolver, IdResolver);
  }

  ~ParserLookupSetup() {
    std::swap(SemaRef.IdResolver, IdResolver);
    // delete IdResolver;

    while (CurScope) {
      Scope *ParentScope = CurScope->getParent();
      delete CurScope;
      CurScope = ParentScope;
    }
  }

  Scope *getCurScope() {
    return CurScope;
  }

  void AddScope(unsigned ScopeFlags) {
    CurScope = new Scope(CurScope, ScopeFlags, SemaRef.PP.getDiagnostics());
  }

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

  void AddDecl(Decl *D) {
    if (NamedDecl *ND = castAsResolveableDecl(D)) {
      CurScope->AddDecl(ND);
      IdResolver->AddDecl(ND);
    }
  }

  void VisitParentsInOrder(DeclContext *DC) {
    if (DeclContext *PCD = DC->getParent())
      VisitParentsInOrder(PCD);

    Visit(DC);
  }

  void MergeWithSemaState() {
    for (Scope *Scope : SemaRef.getTTParseScopes()) {
      AddScope(Scope->getFlags());

      for (Decl *D : Scope->decls())
        AddDecl(D);
    }
  }

  void Visit(DeclContext *S) {
    DeclVisitor<ParserLookupSetup>::Visit(Decl::castFromDeclContext(S));
  }

  void VisitTranslationUnitDecl(TranslationUnitDecl *TUD) {
    AddScope(Scope::DeclScope);

    for (Decl *D : TUD->decls())
      AddDecl(D);
  }

  void VisitNamespaceDecl(NamespaceDecl *NSD) {
    AddScope(Scope::DeclScope);

    for (Decl *D : NSD->decls())
      AddDecl(D);
  }

  void VisitCXXRecordDecl(CXXRecordDecl *RD) {
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

  void VisitFunctionDecl(FunctionDecl *FD) {
    AddScope(Scope::FnScope | Scope::DeclScope | Scope::CompoundStmtScope);

    for (unsigned P = 0, NumParams = FD->getNumParams(); P < NumParams; ++P) {
      ParmVarDecl *Param = FD->getParamDecl(P);

      if (Param->getIdentifier())
        AddDecl(Param);
    }
  }
};

}} // end namespace clang::sema

#endif
