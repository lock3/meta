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
#include "clang/AST/Type.h"
#include "clang/AST/TemplateName.h"

namespace clang {

class Decl;
class Expr;
class NamespaceDecl;
class NestedNameSpecifier;
class Type;
class UnresolvedLookupExpr;

/// Represents a qualified namespace-name.
class QualifiedNamespaceName
{
  // The optional nested name specifier for the namespace.
  NestedNameSpecifier *NNS;

  /// The namespace designated by the operand.
  NamespaceDecl *NS;

public:
  QualifiedNamespaceName(NamespaceDecl *NS)
    : QualifiedNamespaceName(nullptr, NS) { }

  QualifiedNamespaceName(NestedNameSpecifier *NNS, NamespaceDecl *NS)
    : NNS(NNS), NS(NS) { }

  /// Returns the nested-name-specifier, if any.
  NestedNameSpecifier *getQualifier() const { return NNS; }

  /// Returns the designated namespace.
  NamespaceDecl *getNamespace() const { return NS; }
};

/// Represents a namespace-name within a reflection operand.
class NamespaceName
{
  /// This is either an unqualified or qualified namespace name.
  using StorageType = 
    llvm::PointerUnion<NamespaceDecl *, QualifiedNamespaceName *>;

  StorageType Storage;

  explicit NamespaceName(void *Ptr) 
    : Storage(StorageType::getFromOpaqueValue(Ptr)) { }

public:
  enum NameKind {
    /// An unqualified namespace-name.
    Namespace,

    /// A qualified namespace-name.
    QualifiedNamespace,
  };

  explicit NamespaceName(NamespaceDecl *NS) : Storage(NS) { }
  explicit NamespaceName(QualifiedNamespaceName *Q) : Storage(Q) { }

  /// Returns the kind of name stored.
  NameKind getKind() const { 
    if (Storage.is<NamespaceDecl *>())
      return Namespace;
    else
      return QualifiedNamespace;
  }

  /// Returns true if this is qualified.
  bool isQualified() const { return getKind() == QualifiedNamespace; }

  /// Returns the designated namespace, if any.
  NestedNameSpecifier *getQualifier() const {
    if (getKind() == QualifiedNamespace)
      return Storage.get<QualifiedNamespaceName *>()->getQualifier();
    else
      return nullptr;
  }

  /// Returns the designated namespace.
  NamespaceDecl *getNamespace() const { 
    if (getKind() == Namespace)
      return Storage.get<NamespaceDecl *>();
    else
      return Storage.get<QualifiedNamespaceName *>()->getNamespace();
  }

  /// Returns this as an opaque pointer.
  void *getAsVoidPointer() const { 
    return Storage.getOpaqueValue(); 
  }

  /// Returns this as an opaque pointer.
  static NamespaceName getFromVoidPointer(void *P) { 
    return NamespaceName(P); 
  }
};

/// Represents an operand to the reflection operator. 
class ReflectionOperand
{
public:
  enum ReflectionKind {
    Type,
    Template,
    Namespace,
    Expression,
  };

private:
  ReflectionKind Kind;

  /// Points to the representation of the operand. For type operands, this is
  /// the opaque pointer of a QualType. For template-name operands, this is
  /// the opaque pointer for a TemplateName. For namespace operands, this is
  /// a pointer to NamespaceName. For expressions, this is the expression
  /// pointer.
  void *Data;

public:
  ReflectionOperand()
    : Kind(Type), Data()
  { }

  ReflectionOperand(QualType T)
    : Kind(Expression), Data(T.getAsOpaquePtr()) { }

  ReflectionOperand(TemplateName T)
    : Kind(Expression), Data(T.getAsVoidPointer()) { }

  ReflectionOperand(NamespaceName T)
    : Kind(Expression), Data(T.getAsVoidPointer()) { }

  ReflectionOperand(Expr *E)
    : Kind(Expression), Data(E) { }

  /// Returns the kind of reflection.
  ReflectionKind getKind() const { return Kind; }

  /// Returns true if the reflection is invalid.
  bool isInvalid() const { return !Data; }

  /// Returns this as a type operand.
  QualType getAsType() const {
    assert(getKind() == Type && "not a type");
    return QualType::getFromOpaquePtr(Data);
  }

  TemplateName getAsTemplate() const {
    assert(getKind() == Template && "not a template");
    return TemplateName::getFromVoidPointer(Data);
  }

  NamespaceName getAsNamespace() const {
    assert(getKind() == Namespace && "not a namespace");
    return NamespaceName::getFromVoidPointer(Data);
  }

  Expr *getAsExpression() const {
    assert(getKind() == Expression && "not an expression");
    return reinterpret_cast<Expr *>(Data);
  }
};


namespace reflect {

bool isReflectedDeclaration(APValue &Reflection);
bool isReflectedType(APValue &Reflection);
bool isReflectedStatement(APValue &Reflection);
bool isReflectedULE(APValue &Reflection);
bool isNullReflection(APValue &Reflection);

const Decl *getReflectedDeclaration(APValue &Reflection);
const Type *getReflectedType(APValue &Reflection);
const Expr *getReflectedStatement(APValue &Reflection);
const UnresolvedLookupExpr *getReflectedULE(APValue &Reflection);

const Decl *getAsReflectedDeclaration(APValue &Reflection);
const Type *getAsReflectedType(APValue &Reflection);
const Expr *getAsReflectedStatement(APValue &Reflection);
const UnresolvedLookupExpr *getAsReflectedULE(APValue &Reflection);

} // namespace reflect

} // namespace clang

#endif
