//=- LifetimeTypeCategory.h - Diagnose lifetime violations -*- C++ -*-========//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMETYPECATEGORY_H
#define LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMETYPECATEGORY_H

#include "clang/AST/Type.h"
#include "clang/Analysis/Analyses/Lifetime.h"

namespace clang {
namespace lifetime {
/// Returns the type category of the given type
/// If T is a template specialization, it must be instantiated.
/// \post If the returned TypeCategory is Owner or Pointer, then
///       getPointeeType() will return non-null for the same QT.
TypeCategory classifyTypeCategory(QualType QT);

bool isNullableType(QualType QT);

// For primitive types like pointers, references we return the pointee.
// For user defined types the pointee type is determined by the return
// type of operator*, operator-> or operator[]. Since these methods
// might return references, and operator-> returns a pointer, we strip
// off that one extra level of pointer/references.
QualType getPointeeType(QualType QT);

struct CallTypes {
  const FunctionProtoType *FTy = nullptr;
  const CXXRecordDecl *ClassDecl = nullptr;
};

/// Obtains the function prototype (without 'this' pointer) and the type of
/// the object (if MemberCallExpr).
CallTypes getCallTypes(const Expr *CalleeE);

bool isLifetimeConst(const FunctionDecl *FD, QualType Pointee, int ArgNum);
} // namespace lifetime
} // namespace clang

#endif // LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMETYPECATEGORY_H