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
#include "clang/AST/ASTContext.h"
#include "clang/Basic/TypeTraits.h"

namespace clang {

class CXXBaseSpecifier;
class Decl;
class Expr;
class NamespaceDecl;
class NestedNameSpecifier;
class Sema;
class Type;
class UnresolvedLookupExpr;

using ReflectedNamespace =
  llvm::PointerUnion<NamespaceDecl *, TranslationUnitDecl *>;

/// Represents a qualified namespace-name.
class QualifiedNamespaceName {
  /// The namespace designated by the operand.
  ReflectedNamespace NS;

  // The qualifying nested name specifier for the namespace.
  NestedNameSpecifier *NNS;
public:
  QualifiedNamespaceName(
      ReflectedNamespace NS, NestedNameSpecifier *NNS)
    : NS(NS), NNS(NNS) {
    // Namespace must always be set.
    assert(!NS.isNull());
  }

  /// Returns the designated namespace.
  ReflectedNamespace getNamespace() const { return NS; }

  /// Returns the nested-name-specifier, if any.
  NestedNameSpecifier *getQualifier() const { return NNS; }
};

/// Represents a namespace-name within a reflection operand.
class NamespaceName {
  // This is either an a reflected namespace or a qualified namespace name.
  using StorageType =
    llvm::PointerUnion<
        NamespaceDecl *, TranslationUnitDecl *, QualifiedNamespaceName *>;

  StorageType Storage;

  explicit NamespaceName(void *Ptr)
    : Storage(StorageType::getFromOpaqueValue(Ptr)) {
    // Storage must always be set.
    assert(!Storage.isNull());
  }
public:
  enum NameKind {
    /// An unqualified namespace-name.
    Namespace,

    /// A qualified namespace-name.
    QualifiedNamespace
  };

  explicit NamespaceName(ReflectedNamespace NS) {
    if (NS.is<NamespaceDecl *>()) {
      Storage = NS.get<NamespaceDecl *>();
    } else if (NS.is<TranslationUnitDecl *>()) {
      Storage = NS.get<TranslationUnitDecl *>();
    }

    // Storage must always be set.
    assert(!Storage.isNull());
  }

  explicit NamespaceName(QualifiedNamespaceName *Q) : Storage(Q) {
    // Storage must always be set.
    assert(!Storage.isNull());
  }

  /// Returns the kind of name stored.
  NameKind getKind() const {
    if (Storage.is<QualifiedNamespaceName *>())
      return QualifiedNamespace;

    return Namespace;
  }

  /// Returns true if this is qualified.
  bool isQualified() const { return getKind() == QualifiedNamespace; }

  /// Returns the designated namespace, if any.
  NestedNameSpecifier *getQualifier() const {
    if (isQualified())
      return Storage.get<QualifiedNamespaceName *>()->getQualifier();

    return nullptr;
  }

  /// Returns the designated namespace.
  ReflectedNamespace getNamespace() const {
    if (NamespaceDecl *NS = Storage.dyn_cast<NamespaceDecl *>())
      return { NS };

    if (TranslationUnitDecl *TU = Storage.dyn_cast<TranslationUnitDecl *>())
      return { TU };

    return Storage.get<QualifiedNamespaceName *>()->getNamespace();
  }

  /// Returns the designated namespace as a Decl.
  Decl *getNamespaceAsDecl() const {
    ReflectedNamespace ReflNs = getNamespace();

    if (NamespaceDecl *NS = ReflNs.dyn_cast<NamespaceDecl *>())
      return NS;

    return ReflNs.get<TranslationUnitDecl *>();
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

/// Represents an invalid reflection.
struct InvalidReflection {
  // The evaluated error message expr.
  const Expr *ErrorMessage;
};

/// Represents an operand to the reflection operator.
class ReflectionOperand {
public:
  enum ReflectionOpKind {
    Type,        // Begin parseable kinds
    Template,
    Namespace,
    Expression,  // End parseable kinds

    Invalid,
    Declaration,
    BaseSpecifier
  };

private:
  ReflectionOpKind Kind;

  /// Points to the representation of the operand. For type operands, this is
  /// the opaque pointer of a QualType. For template-name operands, this is
  /// the opaque pointer for a TemplateName. For namespace operands, this is
  /// a pointer to NamespaceName. For expressions, this is the expression
  /// pointer.
  void *Data;

public:
  ReflectionOperand()
    : Kind(Invalid)
  { }

  ReflectionOperand(InvalidReflection *IR)
    : Kind(Invalid), Data(IR)
  { }

  ReflectionOperand(QualType T)
    : Kind(Type), Data(T.getAsOpaquePtr()) { }

  ReflectionOperand(TemplateName T)
    : Kind(Template), Data(T.getAsVoidPointer()) { }

  ReflectionOperand(NamespaceName T)
    : Kind(Namespace), Data(T.getAsVoidPointer()) { }

  ReflectionOperand(Expr *E)
    : Kind(Expression), Data(E) { }

  ReflectionOperand(Decl *D)
    : Kind(Declaration), Data(D) { }

  ReflectionOperand(CXXBaseSpecifier *B)
    : Kind(BaseSpecifier), Data(B) { }

  /// Returns the kind of reflection.
  ReflectionOpKind getKind() const { return Kind; }

  /// Returns true if the reflection is invalid.
  bool isInvalid() const { return Kind == Invalid; }

  /// Returns the opaque reflection pointer.
  const void *getOpaqueReflectionValue() const {
    return Data;
  }

  /// Returns the invalid reflection information.
  ///
  /// This can and will be null in most cases.
  InvalidReflection *getAsInvalidReflection() const {
    assert(getKind() == Invalid && "not invalid");
    return reinterpret_cast<InvalidReflection *>(Data);
  }

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

  Decl *getAsDeclaration() const {
    assert(getKind() == Declaration && "not a declaration");
    return reinterpret_cast<Decl *>(Data);
  }

  CXXBaseSpecifier *getAsBaseSpecifier() const {
    assert(getKind() == BaseSpecifier && "not a base specifier");
    return reinterpret_cast<CXXBaseSpecifier *>(Data);
  }
};

struct ReflectionCallback {
  virtual ~ReflectionCallback() { }
  virtual bool EvalTypeTrait(TypeTrait Kind,
                             ArrayRef<TypeSourceInfo *> Args) = 0;
};

enum ReflectionQuery : unsigned;

/// Returns the ReflectionQuery value representing an unknown reflection query.
ReflectionQuery getUnknownReflectionQuery();

unsigned getMinNumQueryArguments(ReflectionQuery Q);
unsigned getMaxNumQueryArguments(ReflectionQuery Q);

/// True if Q is a predicate.
bool isPredicateQuery(ReflectionQuery Q);

/// True if Q returns an associated reflection.
bool isAssociatedReflectionQuery(ReflectionQuery Q);

/// True if Q returns a name.
bool isNameQuery(ReflectionQuery Q);

/// True if Q updates modifiers.
bool isModifierUpdateQuery(ReflectionQuery Q);

enum class AccessModifier : unsigned {
  NotModified,
  Default,
  Public,
  Protected,
  Private
};

enum class StorageModifier : unsigned {
  NotModified,
  Static,
  Automatic,
  ThreadLocal
};

enum class ConstexprModifier : unsigned {
  NotModified,
  Constexpr,
  Consteval,
  Constinit
};

class ReflectionModifiers {
  AccessModifier Access;
  StorageModifier Storage;
  ConstexprModifier Constexpr;
  bool AddExplicit;
  bool AddVirtual;
  bool AddPureVirtual;
  const Expr *NewName;
public:
  ReflectionModifiers()
    : Access(AccessModifier::NotModified),
      Storage(StorageModifier::NotModified),
      Constexpr(ConstexprModifier::NotModified),
      AddExplicit(false), AddVirtual(false),
      AddPureVirtual(false), NewName(nullptr) { }

  void setAccessModifier(AccessModifier Access) {
    this->Access = Access;
  }

  bool modifyAccess() const {
    return Access != AccessModifier::NotModified;
  }

  AccessModifier getAccessModifier() const {
    return Access;
  }

  void setStorageModifier(StorageModifier Storage) {
    assert((Storage == StorageModifier::NotModified
            || Storage == StorageModifier::Static)
           && "Currently only static storage modification is supported");
    this->Storage = Storage;
  }

  bool modifyStorage() const {
    return Storage != StorageModifier::NotModified;
  }

  StorageModifier getStorageModifier() const {
    return Storage;
  }

  void setConstexprModifier(ConstexprModifier Constexpr) {
    this->Constexpr = Constexpr;
  }

  bool modifyConstexpr() const {
    return Constexpr != ConstexprModifier::NotModified;
  }

  ConstexprModifier getConstexprModifier() const {
    return Constexpr;
  }

  void setAddExplicit(bool AddExplicit) {
    this->AddExplicit = AddExplicit;
  }

  bool addExplicit() const {
    return AddExplicit;
  }

  void setAddVirtual(bool AddVirtual) {
    this->AddVirtual = AddVirtual;
  }

  bool addVirtual() const {
    return AddVirtual;
  }

  void setAddPureVirtual(bool AddPureVirtual) {
    this->AddPureVirtual = AddPureVirtual;
  }

  bool addPureVirtual() const {
    return AddPureVirtual;
  }

  void setNewName(const Expr *NewName) {
    this->NewName = NewName;
  }

  bool hasRename() const {
    return NewName != nullptr;
  }

  const Expr *getNewName() const {
    return NewName;
  }

  std::string getNewNameAsString() const;
};

/// The reflection class provides context for evaluating queries.
///
/// FIXME: This might not need diagnostics; we could simply return invalid
/// reflections, which would make the class much, much easier to implement.
class Reflection {
  /// The AST context is needed for global information.
  ASTContext *Ctx;

  /// The reflected entity or construct.
  APValue Ref;

  /// The expression defining the query.
  const Expr *Query;

  /// Points to a vector of diagnostics, to be populated during query
  /// evaluation.
  SmallVectorImpl<PartialDiagnosticAt> *Diag;

public:
  Reflection()
    : Ctx(nullptr), Ref(APValue(RK_invalid, nullptr)), Query(), Diag() {
  }

  /// Construct a reflection that will be used only to observe the
  /// reflected value.
  Reflection(ASTContext &C, const APValue &R)
    : Ctx(&C), Ref(R), Query(), Diag() {
    assert(Ref.isReflectionVariant() && "not a reflection");
  }

  /// Construct a reflection that will be used to evaluate a query.
  ///
  /// DEPRECATED: Avoid using this class for evaluation state,
  /// instead it should be used as a useful wrapper around APValue reflections.
  Reflection(ASTContext &C, const APValue &R, const Expr *Query,
             SmallVectorImpl<PartialDiagnosticAt> *D = nullptr)
    : Ctx(&C), Ref(R), Query(Query), Diag(D) {
    assert(Ref.isReflection() && "not a reflection");
  }

  /// Returns the ASTContext for this reflection.
  ASTContext &getContext() const {
    return *Ctx;
  }

  /// Returns the related query for this reflection, if present.
  const Expr *getQuery() const {
    return Query;
  }

  /// Returns the vector holding diagnostics for query evaluation.
  SmallVectorImpl<PartialDiagnosticAt> *getDiag() const {
    return Diag;
  }

  /// Returns the reflection kind.
  ReflectionKind getKind() const {
    return Ref.getReflectionKind();
  }

  // Returns the opaque reflection pointer.
  const void *getOpaqueValue() const {
    return Ref.getOpaqueReflectionValue();
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

  /// Returns this as an invalid reflection.
  const InvalidReflection *getAsInvalidReflection() const {
    return Ref.getInvalidReflectionInfo();
  }

  /// Returns this as a type.
  QualType getAsType() const {
    return Ref.getReflectedType();
  }

  /// Returns this as a declaration.
  const Decl *getAsDeclaration() const {
    return Ref.getReflectedDeclaration();
  }

  /// If R designates some kind of declaration, either directly, as a type,
  /// or via a reflection, return that declaration.
  const Decl *getAsReachableDeclaration() const;

  /// Returns this as an expression.
  const Expr *getAsExpression() const {
    return Ref.getReflectedExpression();
  }

  /// Returns this as a base class specifier.
  const CXXBaseSpecifier *getAsBase() const {
    return Ref.getReflectedBaseSpecifier();
  }

  /// Returns the modifiers associated with this reflection.
  const ReflectionModifiers &getModifiers() const {
    return Ref.getReflectionModifiers();
  }

  unsigned getOffsetInParent() const {
    return Ref.getReflectionOffset();
  }

  bool hasParent() const {
    return Ref.hasParentReflection();
  }

  Reflection getParent() const {
    return { *Ctx, Ref.getParentReflection() };
  }

  /// Returns the reflected construct designated by Q.
  bool GetAssociatedReflection(ReflectionQuery Q, APValue &Result);

  /// Returns the entity name designated by Q.
  bool GetName(ReflectionQuery Q, APValue &Result);

  /// Updates this reflections reflection modifiers with the
  /// requested changes.
  bool UpdateModifier(ReflectionQuery Q,
                      const ArrayRef<APValue> &ContextualArgs,
                      APValue &Result);

  /// True if A and B reflect the same entity.
  static bool Equal(ASTContext &Ctx, APValue const& X, APValue const& Y);
};

class ReflectionQueryEvaluator {
  /// The AST context is needed for global information.
  ASTContext *Ctx;

  /// Optional callbacks to Sema functionality.
  ReflectionCallback *CB;

  /// The query to evaluate.
  ReflectionQuery Query;

  /// The expression defining the query.
  const Expr *QueryExpr;

  /// Points to a vector of diagnostics, to be populated during query
  /// evaluation.
  SmallVectorImpl<PartialDiagnosticAt> *Diag;
public:
  ReflectionQueryEvaluator(ASTContext &C, ReflectionCallback *CB,
                           ReflectionQuery Q, const Expr *E,
                           SmallVectorImpl<PartialDiagnosticAt> *D)
    : Ctx(&C), CB(CB), Query(Q), QueryExpr(E), Diag(D) {
  }

  /// Returns the ASTContext for this evaluation.
  ASTContext &getContext() const {
    return *Ctx;
  }

  ReflectionCallback *getCallbacks() const {
    return CB;
  }

  ReflectionQuery getQuery() const {
    return Query;
  }

  /// Returns the related query for this evaluation, if present.
  const Expr *getQueryExpr() const {
    return QueryExpr;
  }

  /// Returns the vector holding diagnostics for query evaluation.
  SmallVectorImpl<PartialDiagnosticAt> *getDiag() const {
    return Diag;
  }

  /// Evaluates the predicate designated by Q.
  bool EvaluatePredicate(SmallVectorImpl<APValue> &Args, APValue &Result);
};

bool EvaluateReflection(Sema &S, Expr *E, Reflection &R);

void DiagnoseInvalidReflection(Sema &S, Expr *E, const Reflection &R);

} // namespace clang

#endif
