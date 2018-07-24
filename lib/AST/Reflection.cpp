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
#include "clang/AST/DeclCXX.h"
#include "clang/AST/ASTContext.h"
using namespace clang;

// We need at least 64 bits of pointer value internally. This is also the
// "natural" upper limit of easily working with APSInts.
static_assert(sizeof(std::intptr_t) >= sizeof(std::uint64_t), "");

// Anything that can be directly encoded in the pointer value must have at
// least 8 bits of alignment. We need extra bits to encode the AST kind.
static_assert(alignof(Decl) >= 8, "");
static_assert(alignof(Type) >= 8, "");
static_assert(alignof(Stmt) >= 8, "");
static_assert(alignof(CXXBaseSpecifier) >= 8, "");

APValue Reflection::getConstantValue(ASTContext& Ctx) const {
  APValue V;
  getConstantValue(Ctx, V);
  return V;
}

void Reflection::getConstantValue(ASTContext& Ctx, APValue& Value) const {
  std::intptr_t N = (std::intptr_t)Ptr | Kind;
  Value = APValue(Ctx.MakeIntValue(N, Ctx.getIntPtrType()));
}

void Reflection::putConstantValue(const APValue& Value) {
  assert(Value.isInt() && "Expected an integer value");
  std::intptr_t N = Value.getInt().getExtValue();
  Ptr = reinterpret_cast<void*>(N & ~0x03);
  Kind = static_cast<ReflectionKind>(N & 0x03);
}
