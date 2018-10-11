//===--- Reflection.cpp - Classes for representing reflection ---*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements the Reflection class.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/Reflection.h"

namespace clang {

namespace reflect {

bool isReflectedDeclaration(APValue &Reflection) {
  assert(Reflection.isReflection() && "Not a reflection");
  return Reflection.getReflectionKind() == REK_declaration;
}

bool isReflectedType(APValue &Reflection) {
  assert(Reflection.isReflection() && "Not a reflection");
  return Reflection.getReflectionKind() == REK_type;
}

bool isReflectedStatement(APValue &Reflection) {
  assert(Reflection.isReflection() && "Not a reflection");
  return Reflection.getReflectionKind() == REK_statement;
}

bool isReflectedULE(APValue &Reflection) {
  if (!isReflectedStatement(Reflection))
    return false;
  const Stmt *S = getReflectedStatement(Reflection);
  return S->getStmtClass() == Stmt::UnresolvedLookupExprClass;
}

bool isNullReflection(APValue &Reflection) {
  return Reflection.getReflectionKind() == REK_special
      && Reflection.getReflectedEntity() == nullptr;
}

const Decl *getReflectedDeclaration(APValue &Reflection) {
  assert(isReflectedDeclaration(Reflection) && "Not a declaration");
  auto ReflEntity = Reflection.getReflectedEntity();
  return static_cast<const Decl *>(ReflEntity);
}

const Type *getReflectedType(APValue &Reflection) {
  assert(isReflectedType(Reflection) && "Not a type");
  auto ReflEntity = Reflection.getReflectedEntity();
  return static_cast<const Type *>(ReflEntity);
}

const Stmt *getReflectedStatement(APValue &Reflection) {
  assert(isReflectedStatement(Reflection) && "Not a statement");
  auto ReflEntity = Reflection.getReflectedEntity();
  return static_cast<const Stmt *>(ReflEntity);
}

const UnresolvedLookupExpr *getReflectedULE(APValue &Reflection) {
  assert(isReflectedULE(Reflection) && "Not an unresolved expression");
  auto ReflEntity = Reflection.getReflectedEntity();
  return static_cast<const UnresolvedLookupExpr *>(ReflEntity);
}

const Decl *getAsReflectedDeclaration(APValue &Reflection) {
  return isReflectedDeclaration(Reflection)
       ? getReflectedDeclaration(Reflection) : nullptr;
}

const Type *getAsReflectedType(APValue &Reflection) {
  return isReflectedType(Reflection)
       ? getReflectedType(Reflection) : nullptr;
}

const UnresolvedLookupExpr *getAsReflectedULE(APValue &Reflection) {
  return isReflectedULE(Reflection)
       ? getReflectedULE(Reflection) : nullptr;
}

} // namespace reflect

} // namespace clang
