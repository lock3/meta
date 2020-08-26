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
#include "llvm/ADT/DenseMap.h" 
#include <vector>

using namespace clang;
using namespace sema;

namespace {

  // Returns true if D is a movable input parameter, which is only the case
  // when D is nontrivial.
  //
  // FIXME: This is duplicated in SemaDecl.cpp.
  static bool isMovableInParameter(ASTContext &Ctx, const ParmVarDecl *D)
  {
    QualType T = D->getType();
    if (const auto *PT = dyn_cast<ParameterType>(T))
      if (PT->isInParameter())
        return !InParameterType::isPassByValue(Ctx, PT->getParameterType());
    return false;
  }

  // Returns the referenced parameter if `S` refers to one.
  static const ParmVarDecl *maybeGetInParameter(ASTContext &Ctx, const Stmt *S) {
    if (auto *E = dyn_cast<DeclRefExpr>(S)) {
      if (auto *P = dyn_cast<ParmVarDecl>(E->getDecl()))
        if (isMovableInParameter(Ctx, P))
          return P;
    }
    return nullptr;
  }

  /// Maintains per-block information about the last use of movable input
  /// variables.
  struct LastUseSearch {
    // Vertex colors for a DFS.
    enum class Color {
      Black, White, Gray
    };

    /// Whether the last use of a parameter is known or not.
    ///
    enum class LastUse {
      Unknown, // The last use is not yet known.
      Known,   // The last use is known.
      Indeterminate // There is no definite last use.
    };

    using UseMap = llvm::SmallDenseMap<const Decl *, LastUse>;
    using UseSet = llvm::SmallDenseSet<const Expr *>;

    LastUseSearch(Sema &S, FunctionDecl *D, CFG *G)
        : SemaRef(S), Fn(D), G(G), ColorMap(G->size()) {
      // Mark all nodes unvisited.
      for (CFGBlock *B : *G)
        getColor(B) = Color::White;
    }

    // Returns the color of a block.
    Color& getColor(CFGBlock *B) {
      return ColorMap[B->getBlockID()];
    }

    // Visit a statement (expression) to determine if it constitutes the last
    // use of a statement.
    void visitStmt(int I, CFGStmt Elem, UseMap& Uses) {
      llvm::outs() << "STMT " << I << "\n";
      Elem.dump();
      // Elem.getStmt()->dump();
      ASTContext &Ctx = SemaRef.Context;
      if (const ParmVarDecl *P = maybeGetInParameter(Ctx, Elem.getStmt())) {
        auto Iter = Uses.find(P);
        if (Iter == Uses.end()) {
          // The first reference to parameter is likely to be its last use.
          //
          // FIXME: This is not entirely true. See below.
          Uses.try_emplace(P, LastUse::Known);
          llvm::outs() << "LAST!\n";
          Elem.getStmt()->dump();
          LastUses.insert(cast<DeclRefExpr>(Elem.getStmt()));
        }
      }

      // FIXME: For all other expressions, we might need to check whether
      // the last use was indeterminate or not. For example:
      //
      //    void f(in x) {
      //      return g(x, x);  
      //
      // Neither x can be considered the last use because the order of is 
      // unspecified. In this case, there should be no definite last use of x.
    }

    // Iterate over the elements of a CFG block in reverse. Note that this
    // effectively gives us a preorder traversal of expressions in a statement.
    void visitStatements(CFGBlock *B, UseMap& Uses) {
      int I = 0;
      for (auto Iter = B->rbegin(); Iter != B->rend(); ++Iter) {
        CFGElement Elem = *Iter;
        if (auto StmtElem = Elem.getAs<CFGStmt>()) {
          visitStmt(++I, *StmtElem, Uses);
        }
      }
    }

    // Recursively visit B and all blocks reachable from B.
    //
    // Note that the map is copied from its parent in the tree so that local
    // changes in this and reachable blocks do not modify the paren't view of
    // which expressions are the last use of a parameter.
    void visitBlock(CFGBlock *B, UseMap Uses) {
      getColor(B) = Color::Gray;

      llvm::outs() << "EXAMINE " << B->getBlockID() << '\n';
      B->dump();
      visitStatements(B, Uses);

      for (auto &Adj : B->preds())
        if (getColor(Adj) == Color::White)
          visitBlock(Adj, Uses);

      getColor(B) = Color::Black;
    }

    void start() {
      UseMap Uses;
      visitBlock(&G->getExit(), Uses);
    }

    Sema &SemaRef;
    FunctionDecl *Fn;
    CFG *G;
    std::vector<Color> ColorMap;
    UseSet LastUses;
  };

  // Perform a depth-first search on the CFG, starting at the exit block and
  // proceeding along predecessor paths.
  void findLastUses(Sema &SemaRef, FunctionDecl *D, CFG *G) {
    LastUseSearch S(SemaRef, D, G);
    S.start();
  }
} // namespace

void Sema::computeMoveOnLastUse(FunctionDecl *D) {
  // Construct the analysis context with the default CFG build options.
  AnalysisDeclContext AC(nullptr, D);
  AC.getCFGBuildOptions().setAllAlwaysAdd();

  // CFG *G = AC.getCFG();
  // G->dump(Context.getLangOpts(), true);
  // llvm::outs() << "----------------\n";
  // findLastUses(*this, D, G);
}
