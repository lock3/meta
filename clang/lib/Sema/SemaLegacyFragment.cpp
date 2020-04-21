//===--- SemaLegacyFragment.cpp - Semantic Analysis for Legacy Fragments --===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements semantic rules for the legacy style, implicit
//  capture C++ fragments.
//
//===----------------------------------------------------------------------===//

#include "TreeTransform.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTDiagnostic.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclVisitor.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/Type.h"
#include "clang/Sema/Initialization.h"
#include "clang/Sema/Template.h"
#include "clang/Sema/SemaInternal.h"

using namespace clang;

static Decl *GetRootDeclaration(Expr *E) {
  if (auto *ICE = dyn_cast<ImplicitCastExpr>(E))
    return GetRootDeclaration(ICE->getSubExpr());

  if (auto *ASE = dyn_cast<ArraySubscriptExpr>(E))
    return GetRootDeclaration(ASE->getBase());

  if (auto *DRE = dyn_cast<DeclRefExpr>(E))
    return DRE->getDecl();

  if (auto *ME = dyn_cast<MemberExpr>(E))
    return ME->getMemberDecl();

  llvm_unreachable("failed to find declaration");
}

template<typename T>
static bool isCapturableEntity(T *Entity) {
  QualType EntityTy = Entity->getType();

  if (EntityTy->isPointerType())
    return false;

  if (EntityTy->canDecayToPointerType())
    return false;

  if (EntityTy->isReferenceType())
    return false;

  return true;
}

template<typename T, typename F>
static void FilteredCaptureCB(T *Entity, F CaptureCB) {
  if (isCapturableEntity(Entity))
    CaptureCB(Entity);
}

template<typename F>
static void ExtractDecomposedDecls(Sema &S, DecompositionDecl *D,
                                   F CapturedDeclCB) {
  for (BindingDecl *BD : D->bindings()) {
    FilteredCaptureCB(BD, CapturedDeclCB);
  }
}

// Where E is an expression that's been created by the capture system,
// set VD to the ValueDecl referred to by E.
//
// If this cannot be completed, return true, and diagnose.
static bool getCapturedVariable(Sema &SemaRef, Expr *E, ValueDecl *&VD) {
  VD = cast<ValueDecl>(GetRootDeclaration(E));

  assert(!isa<DecompositionDecl>(VD) &&
      "decomposition decl should already be deconstructed.");

  if (isCapturableEntity(VD))
    return false;

  SemaRef.Diag(E->getExprLoc(), diag::err_invalid_fragment_capture)
      << VD->getType();
  return true;
}

// Find variables to capture in the given scope.
static void FindCapturesInScope(Sema &SemaRef, Scope *S,
                                SmallVectorImpl<ValueDecl *> &Vars) {
  for (Decl *D : S->decls()) {
    if (VarDecl *Var = dyn_cast<VarDecl>(D)) {
      // Only capture locals with initializers.
      //
      // FIXME: If the fragment is in the initializer of a variable, this
      // will also capture that variable. For example:
      //
      //    auto f = <<class: ... >>;
      //
      // The capture list for the fragment will include f. This seems insane,
      // but lambda capture seems to also do this (with some caveats about
      // usage).
      //
      // We can actually detect this case in this implementation because
      // the type must be deduced and we won't have associated the
      // initializer with the variable yet.
      if (!isa<ParmVarDecl>(Var) &&
          !Var->hasInit() &&
          Var->getType()->isUndeducedType())
        continue;

      if (auto *DD = dyn_cast<DecompositionDecl>(D)) {
        ExtractDecomposedDecls(SemaRef, DD, [&] (ValueDecl *DV) { Vars.push_back(DV); });
        return;
      }

      FilteredCaptureCB(cast<ValueDecl>(D), [&] (ValueDecl *DV) { Vars.push_back(DV); });
    }
  }
}

// Search the scope list for captured variables. When S is null, we're
// applying applying a transformation.
static void FindCaptures(Sema &SemaRef, Scope *S, FunctionDecl *Fn,
                         SmallVectorImpl<ValueDecl *> &Vars) {
  assert(S && "Expected non-null scope");

  do {
    FindCapturesInScope(SemaRef, S, Vars);
    if (S->getEntity() == Fn)
      break;
  } while ((S = S->getParent()));
}

/// Construct a reference to each captured value and force an r-value
/// conversion so that we get rvalues during evaluation.
static void ReferenceCaptures(Sema &SemaRef,
                              const SmallVectorImpl<ValueDecl *> &Vars,
                              SmallVectorImpl<Expr *> &Refs) {
  Refs.resize(Vars.size());
  std::transform(Vars.begin(), Vars.end(), Refs.begin(), [&](ValueDecl *D) {
    return new (SemaRef.Context) DeclRefExpr(
        SemaRef.Context, D, false, D->getType(), VK_LValue, D->getLocation());
  });
}

// Create a placeholder for each captured expression in the scope of the
// fragment. For some captured variable 'v', these have the form:
//
//    constexpr auto v = <opaque>;
//
// These are replaced by their values during injection.
static bool CreatePlaceholder(Sema &SemaRef, CXXFragmentDecl *Frag, Expr *E) {
  ValueDecl *Var;
  if (getCapturedVariable(SemaRef, E, Var))
    return true;

  SourceLocation NameLoc = Var->getLocation();
  DeclarationName Name = Var->getDeclName();
  QualType T = SemaRef.Context.DependentTy;
  TypeSourceInfo *TSI = SemaRef.Context.getTrivialTypeSourceInfo(T);
  VarDecl *Placeholder = VarDecl::Create(SemaRef.Context, Frag, NameLoc, NameLoc,
                                         Name, T, TSI, SC_Static);
  Placeholder->setConstexpr(false);
  Placeholder->setImplicit(true);
  Placeholder->setInitStyle(VarDecl::CInit);
  Placeholder->setInit(
      new (SemaRef.Context) OpaqueValueExpr(NameLoc, T, VK_RValue));
  Placeholder->setReferenced(true);
  Placeholder->markUsed(SemaRef.Context);

  Frag->addDecl(Placeholder);

  return false;
}

static bool CreatePlaceholders(Sema &SemaRef, CXXFragmentDecl *Frag,
                               SmallVectorImpl<Expr *> &Captures) {
  bool Invalid = false;

  for (Expr *E : Captures) {
    if (CreatePlaceholder(SemaRef, Frag, E))
      Invalid = true;
  }

  return Invalid;
}

/// Called at the start of a source code fragment to establish the list of
/// automatic variables captured. This is only called by the parser and searches
/// the list of local variables in scope.
void Sema::ActOnCXXLegacyFragmentCapture(SmallVectorImpl<Expr *> &Captures) {
  assert(Captures.empty() && "Captures already specified");

  // Only collect captures within a function.
  //
  // FIXME: It might be better to use the scope, but the flags don't appear
  // to be set right within constexpr declarations, etc.
  if (!isa<FunctionDecl>(CurContext))
    return;

  SmallVector<ValueDecl *, 8> Vars;
  FindCaptures(*this, CurScope, getCurFunctionDecl(), Vars);
  ReferenceCaptures(*this, Vars, Captures);
}

/// Builds a new fragment expression.
ExprResult Sema::ActOnCXXLegacyFragmentExpr(SourceLocation Loc, Decl *Fragment,
                                            SmallVectorImpl<Expr *> &Captures) {
  Diag(Loc, diag::warn_deprecated_fragment);
  return BuildCXXLegacyFragmentExpr(Loc, Fragment, Captures);
}

/// Builds a new fragment expression.
ExprResult Sema::BuildCXXLegacyFragmentExpr(SourceLocation Loc, Decl *Fragment,
                                            SmallVectorImpl<Expr *> &Captures) {
  // Validate the captures, if we can no-longer automatically capture
  // a variable, fail.
  for (Expr *E : Captures) {
    ValueDecl *Var;
    if (getCapturedVariable(*this, E, Var))
      return ExprError();
  }

  CXXFragmentDecl *FD = cast<CXXFragmentDecl>(Fragment);
  return CXXFragmentExpr::Create(Context, Loc, FD, Captures, /*Legacy=*/true);
}

/// Called at the start of a source code fragment to establish the fragment
/// declaration and placeholders.
CXXFragmentDecl *Sema::ActOnStartCXXLegacyFragment(
    Scope *S, SourceLocation Loc, SmallVectorImpl<Expr *> &Captures) {
  CXXFragmentDecl *Fragment = CXXFragmentDecl::Create(Context, CurContext, Loc);
  if (CreatePlaceholders(*this, Fragment, Captures))
    return nullptr;

  if (S)
    PushDeclContext(S, Fragment);

  return Fragment;
}
