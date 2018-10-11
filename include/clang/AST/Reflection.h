//===--- Reflection.h - Classes for representing reflection -----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Defines facilities for representing reflected entities.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_REFLECTION_H
#define LLVM_CLANG_AST_REFLECTION_H

#include "clang/AST/APValue.h"
#include "clang/AST/Expr.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Type.h"
#include "clang/AST/ExprCXX.h"
#include "llvm/ADT/DenseMapInfo.h"

namespace clang {

namespace reflect {

bool isReflectedDeclaration(APValue &Reflection);
bool isReflectedType(APValue &Reflection);
bool isReflectedStatement(APValue &Reflection);
bool isReflectedULE(APValue &Reflection);
bool isNullReflection(APValue &Reflection);

const Decl *getReflectedDeclaration(APValue &Reflection);
const Type *getReflectedType(APValue &Reflection);
const Stmt *getReflectedStatement(APValue &Reflection);
const UnresolvedLookupExpr *getReflectedULE(APValue &Reflection);

const Decl *getAsReflectedDeclaration(APValue &Reflection);
const Type *getAsReflectedType(APValue &Reflection);
const UnresolvedLookupExpr *getAsReflectedULE(APValue &Reflection);

} // namespace reflect

} // namespace clang

#endif
