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
  static bool isMovableInParameter(ParmVarDecl *D)
  {
    QualType T = D->getType();
    if (const auto *PT = dyn_cast<ParameterType>(T)) {
      T = PT->getParameterType();
      if (CXXRecordDecl *Class = T->getAsCXXRecordDecl())
        return !Class->canPassInRegisters();
    }
    return false;
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
    struct LastUse {
      enum State {
        Unknown, // The last use is not yet known.
        Known,   // The last use is known.
        Indeterminate // There is no definite last use.
      };
      State S = Unknown;
      Expr *E = nullptr;
    };

    LastUseSearch(FunctionDecl *D, CFG *G) : Fn(D), G(G), ColorMap(G->size()) {
      // Mark all nodes unvisited.
      for (CFGBlock *B : *G)
        getColor(B) = Color::White;
      
      for (std::size_t I = 0; I < Fn->getNumParams(); ++I) {
        ParmVarDecl *P = Fn->getParamDecl(I);
        if (isMovableInParameter(P))
          UseMap[P] = LastUse();
      }
    }

    // Returns the color of a block.
    Color& getColor(CFGBlock *B) {
      return ColorMap[B->getBlockID()];
    }

    /// Called when a vertex is first visited.
    void startBlock(CFGBlock *B) {
      getColor(B) = Color::Gray;

      llvm::outs() << "EXAMINE " << B->getBlockID() << '\n';
      B->dump();
      visitStatements(B);
    }

    /// Can when a vertex is done being visited.
    void finishBlock(CFGBlock *B) {
      getColor(B) = Color::Black;
    }

    // Search statements in reverse order, looking for the last use of
    // a declaration.
    void visitStatements(CFGBlock *B) {
      for (auto Iter = B->rbegin(); Iter != B->rend(); ++Iter) {
        CFGElement E = *Iter;
        llvm::outs() << "ELEMENT\n";
        E.dump();
      }
    }

    // Recursively visit B and all blocks reachable from B.
    void visitBlock(CFGBlock *B) {
      startBlock(B);
      for (auto &Adj : B->preds())
        if (getColor(Adj) == Color::White)
          visitBlock(Adj);
      finishBlock(B);
    }

    FunctionDecl *Fn;
    CFG *G;
    std::vector<Color> ColorMap;
    llvm::SmallDenseMap<ParmVarDecl *, LastUse> UseMap;
  };

  // Recursively visit B in a reverse DFS.
  void findLastUses(LastUseSearch& S, CFGBlock *B) {
    S.visitBlock(B);
  }

  // Perform a depth-first search on the CFG, starting at the exit block and
  // proceeding along predecessor paths.
  void findLastUses(FunctionDecl *D, CFG *G) {
    LastUseSearch S(D, G);
    findLastUses(S, &G->getExit());
  }
}

void Sema::computeMoveOnLastUse(FunctionDecl *D) {
  // Construct the analysis context with the default CFG build options.
  AnalysisDeclContext AC(nullptr, D);
  AC.getCFGBuildOptions().setAllAlwaysAdd();

  CFG *G = AC.getCFG();
  // G->dump(Context.getLangOpts(), true);

  // findLastUses(D, G);
}
