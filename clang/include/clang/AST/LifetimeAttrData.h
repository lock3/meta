
//===--- LifetimeAttrData.h - Classes for lifetime attributes ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines classes that are used by lifetime attributes.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_LIFETIMEATTRDATA_H
#define LLVM_CLANG_AST_LIFETIMEATTRDATA_H
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include <set>

namespace clang {

class MaterializeTemporaryExpr;

/// This represents an abstract memory location that is used in the lifetime
/// contract representation.
struct ContractVariable {
  ContractVariable(const VarDecl *PVD, int Deref = 0) : Var(PVD) {
    assert(PVD);
    deref(Deref);
  }
  ContractVariable(const Expr *E) : Var(E) {}
  ContractVariable(const RecordDecl *RD) : Var(RD) { assert(RD); }

  static ContractVariable returnVal() {
    return ContractVariable(static_cast<const Expr *>(nullptr));
  }

  bool operator==(const ContractVariable &O) const {
    return Var == O.Var && FDs == O.FDs;
  }

  bool operator!=(const ContractVariable &O) const { return !(*this == O); }

  bool operator<(const ContractVariable &O) const {
    if (Var != O.Var)
      return Var < O.Var;
    if (FDs.size() != O.FDs.size())
      return FDs.size() < O.FDs.size();

    for (auto I = FDs.begin(), J = O.FDs.begin(); I != FDs.end(); ++I, ++J) {
      if (*I != *J)
        return std::less<const FieldDecl *>()(*I, *J);
    }
    return false;
  }

  bool isThisPointer() const { return Var.is<const RecordDecl *>(); }

  const ParmVarDecl *asParmVarDecl() const {
    return dyn_cast_or_null<ParmVarDecl>(Var.dyn_cast<const VarDecl *>());
  }

  const RecordDecl *asThis() const {
    return Var.dyn_cast<const RecordDecl *>();
  }

  bool isReturnVal() const {
    return Var.is<const Expr *>() && Var.get<const Expr *>() == nullptr;
  }

  // Chain of field accesses starting from VD. Types must match.
  void addFieldRef(const FieldDecl *FD) { FDs.push_back(FD); }

  ContractVariable &deref(int Num = 1) {
    while (Num--)
      FDs.push_back(nullptr);
    return *this;
  }

protected:
  llvm::PointerUnion<const VarDecl *, const Expr *, const RecordDecl *> Var;

  /// Possibly empty list of fields and deref operations on the base.
  /// The First entry is the field on base, next entry is the field inside
  /// there, etc. Null pointers represent a deref operation.
  llvm::SmallVector<const FieldDecl *, 4> FDs;
};

/// A points-to set that can contain the following locations:
/// - null
/// - static
/// - invalid
/// - variables
/// It a Pset contains non of that, its "unknown".
struct ContractPSet {
  ContractPSet(const std::set<ContractVariable> &Vs = {},
               bool ContainsNull = false)
      : ContainsNull(ContainsNull), ContainsInvalid(false),
        ContainsStatic(false), Vars(Vs) {}
  unsigned ContainsNull : 1;
  unsigned ContainsInvalid : 1;
  unsigned ContainsStatic : 1;
  std::set<ContractVariable> Vars;

  void merge(const ContractPSet &RHS) {
    ContainsNull |= RHS.ContainsNull;
    ContainsStatic |= RHS.ContainsStatic;
    ContainsInvalid |= RHS.ContainsInvalid;
    Vars.insert(RHS.Vars.begin(), RHS.Vars.end());
  }

  bool isEmpty() const {
    return Vars.empty() && !ContainsNull && !ContainsInvalid && !ContainsStatic;
  }
};
} // namespace clang
#endif
