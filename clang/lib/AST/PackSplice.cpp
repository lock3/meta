//===- PackSplice.cpp - C++ Pack Splice Representation --------------------===//
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

#include "clang/AST/PackSplice.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Expr.h"

using namespace clang;

static bool computeIsDependent(ExprDependence Depends) {
  return Depends & ~ExprDependence::UnexpandedPack;
}

static bool computeIsDependent(Expr *Operand) {
  return computeIsDependent(Operand->getDependence());
}

static bool computeIsDependent(llvm::ArrayRef<Expr *> Expansions) {
  ExprDependence Depends = ExprDependence::None;
  for (Expr *SubOperand : Expansions) {
    Depends |= SubOperand->getDependence();
  }
  return computeIsDependent(Depends);
}

PackSplice::PackSplice(Expr *Operand)
  : Operand(Operand), Kind(Unexpanded),
    DependentOperands(computeIsDependent(Operand)), NumExpansions(0) { }

PackSplice::PackSplice(Expr *Operand, llvm::ArrayRef<Expr *> Expansions)
  : Operand(Operand), Kind(Expanded),
    DependentOperands(computeIsDependent(Expansions)),
    NumExpansions(Expansions.size()) {
  assert(!computeIsDependent(Operand) &&
         "dependent operand for expanded pack splice?");

  Expr * const *ExpansionPtr = Expansions.data();
  std::uninitialized_copy(ExpansionPtr, ExpansionPtr + NumExpansions,
                          getTrailingObjects<Expr *>());
}

PackSplice::PackSplice(const PackSplice &PS) {
  // FIXME: This overload is needed for PropertiesBase.td
  // to function properly.
  //
  // Likely the correct fix is to fix the codegeneration associated
  // with PropertiesBase.td
  llvm_unreachable("unimplemented");
}

PackSplice *PackSplice::Create(const ASTContext &Ctx, Expr *Operand) {
  return new (Ctx) PackSplice(Operand);
}

PackSplice *PackSplice::Create(const ASTContext &Ctx, Expr *Operand,
                               llvm::ArrayRef<Expr *> Expansions) {
  auto *Buff = Ctx.Allocate(totalSizeToAlloc<Expr *>(Expansions.size()));
  return new (Buff) PackSplice(Operand, Expansions);
}

void PackSplice::Profile(llvm::FoldingSetNodeID &ID,
                         const ASTContext &Context) const {
  getOperand()->Profile(ID, Context, true);

  if (!isExpanded())
    return;

  ID.AddInteger(getNumExpansions());
  for (Expr *SubOperand : getExpansions())
    SubOperand->Profile(ID, Context, true);
}
