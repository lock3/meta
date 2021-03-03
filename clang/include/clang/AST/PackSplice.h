//===- PackSplice.h - C++ Pack Splice Representation ------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines the PackSplice class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_PACKSPLICE_H
#define LLVM_CLANG_AST_PACKSPLICE_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/FoldingSet.h"
#include "llvm/Support/TrailingObjects.h"

namespace clang {

class ASTContext;
class Expr;

class PackSplice final : private llvm::TrailingObjects<PackSplice, Expr *> {
  friend TrailingObjects;

  Expr *Operand;
  unsigned Kind : 1;
  unsigned DependentOperands : 1;
  unsigned NumExpansions;

  PackSplice(Expr *Operand);
  PackSplice(Expr *Operand, llvm::ArrayRef<Expr *> Expansions);

public:
  PackSplice(const PackSplice &PS);

  // We use an enum representation to ease integration with table gen.
  enum SpliceKind : unsigned {
    Unexpanded,
    Expanded
  };

  static PackSplice *Create(const ASTContext &Ctx, Expr *Operand);
  static PackSplice *Create(const ASTContext &Ctx, Expr *Operand,
                            llvm::ArrayRef<Expr *> Expansions);

  Expr *getOperand() const {
    return Operand;
  }

  SpliceKind getKind() const {
    return static_cast<SpliceKind>(Kind);
  }

  bool isExpanded() const {
    return getKind() == Expanded;
  }

  bool hasDependentOperands() const {
    return DependentOperands;
  }

  unsigned getNumExpansions() const {
    assert(isExpanded() && "Pack splice not expanded");
    return NumExpansions;
  }

  Expr *getExpansion(unsigned I) const {
    assert(isExpanded() && "Pack splice not expanded");
    assert(0 <= I && I < getNumExpansions() && "index out of bounds");
    return getTrailingObjects<Expr *>()[I];
  }

  llvm::ArrayRef<Expr *> getExpansions() const {
    assert(isExpanded() && "Pack splice not expanded");
    return llvm::makeArrayRef(getTrailingObjects<Expr *>(), getNumExpansions());
  }

  void Profile(llvm::FoldingSetNodeID &ID, const ASTContext &Context) const;
};

} // namespace clang

#endif // LLVM_CLANG_AST_PACKSPLICE_H
