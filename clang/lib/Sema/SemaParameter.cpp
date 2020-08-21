//===--- ScopeInfo.cpp - Information about a semantic context -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements FunctionScopeInfo and its subclasses, which contain
// information about a single function, block, lambda, or method body.
//
//===----------------------------------------------------------------------===//

#include "clang/Sema/Sema.h"
#include "clang/Analysis/AnalysisDeclContext.h"
#include "clang/Analysis/CFG.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"

using namespace clang;
using namespace sema;

void Sema::computeMoveOnLastUse(FunctionDecl *D)
{
  // Construct the analysis context with the default CFG build options.
  AnalysisDeclContext AC(nullptr, D);
  AC.getCFGBuildOptions().setAllAlwaysAdd();

  CFG *Graph = AC.getCFG();
  Graph->dump(Context.getLangOpts(), true);
}
