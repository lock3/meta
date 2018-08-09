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
#include "llvm/ADT/DenseMapInfo.h"

namespace clang {

class UnresolvedLookupExpr;

/// \brief The kind of construct reflected. The corresponding AST objects
/// for these constructs MUST have 8-bit aligned.
///
/// \todo Include 
enum ReflectionKind { 
  /// \brief Used internally to represent empty reflections or values that
  /// cannot be encoded directly in the address. If the corresponding
  /// pointer value is null, then the reflection is empty. Otherwise, the
  /// corresponding value is a key in lookup table (not implemented).
  REK_special = 0,
  
  /// \brief A reflection of a named entity, possibly a namespace. Note
  /// that user-defined types are reflected as declarations, not types.
  /// Corresponds to an object of type Decl.
  REK_declaration = 1,

  /// \brief A reflection of a non-user-defined type. Corresponds to
  /// an object of type Type.
  REK_type = 2,

  /// \brief A reflection of a statement or expression. Corresponds to
  /// an object of type Expr.
  REK_statement = 3,

  /// \brief A base class specifier. Corresponds to an object of type
  /// CXXBaseSpecifier.
  REK_base_specifier = 4,
};

/// Information about a reflected construct.
class Reflection {
private:
  ReflectionKind Kind;
  const void* Ptr;

  /// Initialize the reflection. If the pointer is null, then ensure that
  /// the entire entity is going to be null (i.e., special with a null ptr).
  Reflection(ReflectionKind K, const void* P)
    : Kind(P ? K : REK_special), Ptr(P) 
  { }

public:
  /// \brief Constructs an empty reflection.
  Reflection() 
    : Kind(REK_special), Ptr(nullptr) { }

  /// \brief Constructs a reflection of a declaration.
  Reflection(const Decl* D)
    : Reflection(REK_declaration, D) { }

  /// \brief Constructs a reflection of a type.
  Reflection(const Type* T)
    : Reflection(REK_type, T) { }

  /// \brief Constructs a reflection of a statement.
  Reflection(const Stmt *S)
    : Reflection(REK_statement, S) { }

  /// \brief Constructs a reflection of a base class specifier.
  Reflection(const CXXBaseSpecifier *B)
    : Reflection(REK_base_specifier, B) { }

  /// \brief Constructs a reflection from an encoded constant value.
  Reflection(const APValue& V) {
    putConstantValue(V);
  }

  /// \brief Creates a reflection from an entity kind a pointer.
  static Reflection FromKindAndPtr(ReflectionKind K, void* P) { 
    return {K, P}; 
  }

  /// \brief Converts to true when the reflection is valid.
  explicit operator bool() const { return !isNull(); }

  /// \brief The kind of construct reflected.
  ReflectionKind getKind() const { return Kind; }

  /// \brief The opaque pointer.
  const void* getOpaquePointer() const { return Ptr; }

  /// \brief True if the reflection is null.
  bool isNull() const { return Kind == REK_special && Ptr == nullptr; }

  /// \brief True if this is a reflected declaration.
  bool isDeclaration() const { return Kind == REK_declaration; }
  
  /// \brief Returns true if this is a reflected type.
  bool isType() const { return Kind == REK_type; }

  /// \brief Returns true if this is a reflected statement.
  bool isStatement() const { return Kind == REK_statement; }

  /// \brief Returns true if this is a reflected base class specifier.
  bool isBaseSpecifier() const { return Kind == REK_base_specifier; }

  /// \brief Returns true if this is a reflected dependent expression.
  bool isUnresolved() const {    
    if(!isStatement())
      return false;
    const Stmt *S = getStatement();
    return S->getStmtClass() == Stmt::UnresolvedLookupExprClass;
  }

  /// \brief The reflected declaration.
  const Decl *getDeclaration() const{ 
    assert(isDeclaration() && "Not a declaration");
    return (const Decl*)Ptr;
  }

  /// \brief The reflected declaration or null if not a declaration.
  const Decl *getAsDeclaration() const {
    return isDeclaration() ? getDeclaration() : nullptr;
  }

  /// \brief The reflected type.
  const Type *getType() const { 
    assert(isType() && "Not a type");
    return (const Type*)Ptr;
  }

  /// \brief The reflected type or null if not a type.
  const Type* getAsType() const {
    return isType() ? getType() : nullptr;
  }

  /// \brief The reflected statement
  const Stmt *getStatement() const { 
    assert(isStatement() && "Not a statement");
    return (const Stmt*)Ptr;
  }

  /// \brief The reflected statement or null if not a type.
  const Stmt* getAsStatement() const {
    return isStatement() ? getStatement() : nullptr;
  }

  /// \brief The reflected type.
  const CXXBaseSpecifier *getBaseSpecifier() const { 
    assert(isBaseSpecifier() && "Not a base class specifier");
    return (const CXXBaseSpecifier*)Ptr;
  }

  /// \brief The reflected type or null if not a type.
  const CXXBaseSpecifier *getAsBaseSpecifier() const {
    return isBaseSpecifier() ? getBaseSpecifier() : nullptr;
  }

  /// \brief The reflected dependent decl.
  const UnresolvedLookupExpr *getUnresolved() const {
    assert(isUnresolved() && "Not an unresolved expression.");

    return (const UnresolvedLookupExpr*)Ptr;
  }

  /// \brief The reflected dependent decl or null if not a dependent decl.
  const UnresolvedLookupExpr *getAsUnresolved() const {
    return isUnresolved() ? getUnresolved() : nullptr;
  }

  /// Returns the metadata object for the reflection. This is used to construct
  /// the compile-time value of a reflection.
  APValue getConstantValue(ASTContext &Ctx) const;

  /// Sets Value to the value of the reflection.
  void getConstantValue(ASTContext &Ctx, APValue& Value) const;

  /// Sets this object from an encoded constant value.
  void putConstantValue(const APValue& Value);
};

}  // end namespace clang

namespace llvm {
// Specialize DenseMapInfo for reflections. Reflections are hashed and compared
// on the basis of their pointer value only. The kind is implied by the value
// of that pointer (i.e., it is not possible to have two entries reflections
// with the same address, but different kinds).
template<>
struct DenseMapInfo<clang::Reflection> {
  using Reflection = clang::Reflection;
  using Base = DenseMapInfo<void*>;
  static Reflection getEmptyKey() {
    return Reflection::FromKindAndPtr(clang::REK_special, Base::getEmptyKey());
  }
  static Reflection getTombstoneKey() {
    return Reflection::FromKindAndPtr(clang::REK_special, Base::getTombstoneKey());
  }
  static unsigned getHashValue(const Reflection &Val) {
    return Base::getHashValue(Val.getOpaquePointer());
  }
  static bool isEqual(const Reflection &LHS, const Reflection &RHS) {
    return LHS.getOpaquePointer() == RHS.getOpaquePointer();
  }
};
} // namespace llvm

#endif
