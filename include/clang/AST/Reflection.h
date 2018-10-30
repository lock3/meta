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

class CXXBaseSpecifier;
class CXXReflectionTraitExpr;
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
    : Kind(Type), Data(T.getAsOpaquePtr()) { }

  ReflectionOperand(TemplateName T)
    : Kind(Template), Data(T.getAsVoidPointer()) { }

  ReflectionOperand(NamespaceName T)
    : Kind(Namespace), Data(T.getAsVoidPointer()) { }

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

enum ReflectionQuery {
  RQ_unknown,

  RQ_is_invalid,
  RQ_is_entity,
  RQ_is_unnamed,

  // Objects, references, bitfields, and functions
  RQ_is_variable,
  RQ_is_enumerator,
  RQ_is_static_data_member,
  RQ_is_static_member_function,
  RQ_is_nonstatic_data_member,
  RQ_is_bitfield,
  RQ_is_nonstatic_member_function,
  RQ_is_constructor,
  RQ_is_destructor,
  
  // Types
  RQ_is_type,
  RQ_is_class,
  RQ_is_union,
  RQ_is_enum,
  RQ_is_scoped_enum,
  RQ_is_void,
  RQ_is_null_pointer,
  RQ_is_integral,
  RQ_is_floating_point,
  RQ_is_array,
  RQ_is_pointer,
  RQ_is_lvalue_reference,
  RQ_is_rvalue_reference,
  RQ_is_member_object_pointer,
  RQ_is_member_function_pointer,
  RQ_is_closure,

  // Namespaces and aliases
  RQ_is_namespace,
  RQ_is_namespace_alias,
  RQ_is_type_alias,

  // Templates and specializations
  RQ_is_template,
  RQ_is_class_template,
  RQ_is_alias_template,
  RQ_is_function_template,
  RQ_is_variable_template,
  RQ_is_member_function_template,
  RQ_is_static_member_function_template,
  RQ_is_nonstatic_member_function_template,
  RQ_is_constructor_template,
  RQ_is_destructor_template,
  RQ_is_concept,
  RQ_is_specialization,
  RQ_is_partial_specialization,
  RQ_is_explicit_specialization,
  RQ_is_implicit_instantiation,
  RQ_is_explicit_instantiation,

  // Base class specifiers
  RQ_is_direct_base,
  RQ_is_virtual_base,

  // Parameters
  RQ_is_function_parameter,
  RQ_is_template_parameter,
  RQ_is_type_template_parameter,
  RQ_is_nontype_template_parameter,
  RQ_is_template_template_parameter,

  // Expressions
  RQ_is_expression,
  RQ_is_lvalue,
  RQ_is_xvalue,
  RQ_is_rvalue,

  // Scope
  RQ_is_local,
  RQ_is_class_member,
  
  // Traits
  RQ_get_variable_traits,
  RQ_get_function_traits,
  RQ_get_namespace_traits,
  RQ_get_linkage_traits,
  RQ_get_access_traits,

  // Associated reflections
  RQ_get_entity,
  RQ_get_parent,
  RQ_get_type,
  RQ_get_this_ref_type,
  
  // Traversal
  RQ_get_begin,
  RQ_get_end,

  // Name
  RQ_get_name,
  RQ_get_display_name,

  // Labels for kinds of queries. These need to be updated when new
  // queries are added.

  // Predicates -- these return bool.
  RQ_first_predicate = RQ_is_invalid,
  RQ_last_predicate = RQ_is_class_member,
  // Traits -- these return unsigned.
  RQ_first_trait = RQ_get_variable_traits,
  RQ_last_trait = RQ_get_access_traits,
  // Associated reflections -- these return meta::info.
  RQ_first_assoc = RQ_get_entity,
  RQ_last_assoc = RQ_get_end,
  // Names -- these return const char*
  RQ_first_name = RQ_get_name,
  RQ_last_name = RQ_get_display_name,
};

/// True if Q is a predicate.
inline bool isPredicateQuery(ReflectionQuery Q) {
  return RQ_first_predicate <= Q && Q <= RQ_last_predicate;
}

/// True if Q returns trait information.
inline bool isTraitQuery(ReflectionQuery Q) {
  return RQ_first_trait <= Q && Q <= RQ_last_trait;
}

/// True if Q returns an associated reflection.
inline bool isAssociatedReflectionQuery(ReflectionQuery Q) {
  return RQ_first_assoc <= Q && Q <= RQ_last_assoc;
}

/// True if Q returns a name.
inline bool isNameQuery(ReflectionQuery Q) {
  return RQ_first_name <= Q && Q <= RQ_last_name;
}

/// The reflection class provides context for evaluating queries.
struct Reflection {
  /// The AST context is needed for global information.
  ASTContext *Ctx;

  /// The reflected entity or construct.
  const APValue Ref;

public:
  /// The expression defining the query.
  const CXXReflectionTraitExpr *Query;

  /// Points to a vector of diagnostics, to be populated during query
  /// evaluation.
  SmallVectorImpl<PartialDiagnosticAt> *Diag;

  Reflection()
    : Ctx(nullptr), Ref(APValue(RK_invalid, nullptr)), Query(), Diag() {
  }

  /// Construct a reflection that will be used only to observe the
  /// reflected value.
  Reflection(ASTContext &C, const APValue &R)
    : Ctx(&C), Ref(R), Query(), Diag() {
    assert(Ref.isReflection() && "not a reflection");
  }

  /// Construct a reflection that will be used to evaluate a query.
  Reflection(ASTContext &C, const APValue &R, const CXXReflectionTraitExpr *E,
             SmallVectorImpl<PartialDiagnosticAt> *D = nullptr)
    : Ctx(&C), Ref(R), Query(E), Diag(D) {
    assert(Ref.isReflection() && "not a reflection");
  }

  ReflectionKind getKind() const {
    return Ref.getReflectionKind();
  }

  /// True if this is the invalid reflection.
  bool isInvalid() const {
    return Ref.isInvalidReflection();
  }

  /// True if this reflects a type.
  bool isType() const {
    return getKind() == RK_type;
  }

  /// True if this reflects a declaration.
  bool isDeclaration() const {
    return getKind() == RK_declaration;
  }

  /// True if this reflects an expression.
  bool isExpression() const {
    return getKind() == RK_expression;
  }

  /// True if this reflects a base class specifier.
  bool isBase() const {
    return getKind() == RK_base_specifier;
  }

  /// Returns this as a type.
  QualType getAsType() const {
    return Ref.getReflectedType();
  }

  /// Returns this as a declaration.
  const Decl *getAsDeclaration() const {
    return Ref.getReflectedDeclaration();
  }

  /// Returns this as an expression.
  const Expr *getAsExpression() const {
    return Ref.getReflectedExpression();
  }

  /// Returns this as a base class specifier.
  const CXXBaseSpecifier *getAsBase() const {
    return Ref.getReflectedBaseSpecifier();
  }

  /// Evaluates the predicate designated by Q.
  bool EvaluatePredicate(ReflectionQuery Q, APValue &Result);

  /// Returns the traits designated by Q.
  bool GetTraits(ReflectionQuery Q, APValue &Result);

  /// Returns the reflected construct designated by Q.
  bool GetAssociatedReflection(ReflectionQuery Q, APValue &Result);

  /// Returns the entity name designated by Q.
  bool GetName(ReflectionQuery, APValue &Result);
};

} // namespace clang

#endif
