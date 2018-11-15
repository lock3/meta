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

#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTDiagnostic.h"
#include "clang/AST/Reflection.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/LocInfoType.h"
using namespace clang;

/// Returns an APValue-packaged truth value.
static APValue MakeBool(ASTContext *C, bool B) {
  return APValue(C->MakeIntValue(B, C->BoolTy));
}

/// Sets result to the truth value of B and returns true.
static bool SuccessBool(const Reflection &R, APValue &Result, bool B) {
  Result = MakeBool(R.Ctx, B);
  return true;
}

static bool SuccessTrue(const Reflection &R, APValue &Result) {
  return SuccessBool(R, Result, true);
}

static bool SuccessFalse(const Reflection &R, APValue &Result) {
  return SuccessBool(R, Result, false);
}

// Returns false, possibly saving the diagnostic.
static bool Error(const Reflection &R,
                  diag::kind Diag = diag::note_reflection_not_defined) {
  if (R.Diag) {
    // FIXME: We could probably do a better job with the location.
    SourceLocation Loc = R.Query->getExprLoc();
    PartialDiagnostic PD(Diag, R.Ctx->getDiagAllocator());
    R.Diag->push_back(std::make_pair(Loc, PD));
  }
  return false;
}

/// Returns the TypeDecl for a reflected Type, if any.
static const TypeDecl *getAsTypeDecl(const Reflection &R) {
  if (R.isType()) {
    QualType T = R.getAsType();

    // See through location types.
    if (const LocInfoType *LIT = dyn_cast<LocInfoType>(T))
      T = LIT->getType();

    if (const TagDecl *TD = T->getAsTagDecl())
      return TD;

    // FIXME: Handle alias types.
  }
  return nullptr;
}

/// Returns the entity designate by the expression E.
///
/// FIXME: If E is a call expression, return the resolved function.
static const ValueDecl *getEntityDecl(const Expr *E) {
  if (const DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(E))
    return DRE->getDecl();
  return nullptr;
}

/// If R designates some kind of declaration, either directly, as a type,
/// or via a reflection, return that declaration.
static const Decl *getReachableDecl(const Reflection &R) {
  if (const TypeDecl *TD = getAsTypeDecl(R))
    return TD;
  if (R.isDeclaration())
    return R.getAsDeclaration();
  if (R.isExpression())
    return getEntityDecl(R.getAsExpression());
  return nullptr;
}

/// Returns the designated value declaration if reachable through
/// the reflection.
static const ValueDecl *getReachableValueDecl(const Reflection &R) {
  if (const Decl *D = getReachableDecl(R))
    return dyn_cast<ValueDecl>(D);
  return nullptr;
}

/// A helper class to manage conditions involving types.
struct MaybeType {
  MaybeType(QualType T) : Ty(T) { }

  explicit operator bool() const { return !Ty.isNull(); }

  operator QualType() const {
    assert(!Ty.isNull());
    return Ty;
  }

  const Type* operator->() const { return Ty.getTypePtr(); }

  QualType operator*() const { return Ty; }

  QualType Ty;
};

/// Returns a type if one is reachable from R. If an entity is reachable from
/// R, this returns the declared type of the entity (a la decltype).
///
/// Note that this does not get the canonical type.
QualType getReachableType(const Reflection &R) {
  if (R.isType()) {
    QualType T = R.getAsType();

    // See through "location types".
    if (const LocInfoType *LIT = dyn_cast<LocInfoType>(T))
      T = LIT->getType();

    return T;
  }

  if (const ValueDecl *VD = getReachableValueDecl(R))
    return VD->getType();

  return QualType();
}

/// Returns the reachable canonical type. This is used for queries concerned
/// with type entities rather than e.g., aliases.
QualType getReachableCanonicalType(const Reflection &R) {
  QualType T = getReachableType(R);
  if (T.isNull())
    return T;
  return R.Ctx->getCanonicalType(T);
}

/// Returns true if R is an invalid reflection.
static bool isInvalid(const Reflection &R, APValue &Result) {
  return SuccessBool(R, Result, R.isInvalid());
}

/// Sets Result to true if R reflects an entity.
static bool isEntity(const Reflection &R, APValue &Result) {
  if (R.isType())
    // Types are entities.
    return SuccessTrue(R, Result);

  if (R.isDeclaration()) {
    const Decl *D = R.getAsDeclaration();

    if (isa<ValueDecl>(D))
      // Values, objects, references, functions, enumerators, class members,
      // and bit-fields are entities.
      return SuccessTrue(R, Result);

    if (isa<TemplateDecl>(D))
      // Templates are entities (but not template template parameters).
      return SuccessBool(R, Result, !isa<TemplateTemplateParmDecl>(D));

    if (isa<NamespaceDecl>(D))
      // Namespaces are entities.
      return SuccessTrue(R, Result);

    // FIXME: How is a pack an entity?
  }
  return SuccessFalse(R, Result);
}

/// Returns true if R is unnamed.
static bool isUnnamed(const Reflection &R, APValue &Result) {
  if (const Decl *D = R.getAsDeclaration()) {
    if (const NamedDecl *ND = dyn_cast<NamedDecl>(D))
      return SuccessBool(R, Result, ND->getIdentifier() == nullptr);
  }
  return Error(R);
}

/// Returns true if R designates a variable.
static bool isVariable(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R))
    return SuccessBool(R, Result, isa<VarDecl>(D));
  return SuccessFalse(R, Result);
}

/// Returns true if R designates an enumerator.
static bool isEnumerator(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R))
    return SuccessBool(R, Result, isa<EnumConstantDecl>(D));
  return SuccessFalse(R, Result);
}

/// Returns the reflected member function.
static const CXXMethodDecl *getAsMemberFunction(const Reflection &R) {
  if (const Decl *D = getReachableDecl(R))
    return dyn_cast<CXXMethodDecl>(D);
  return nullptr;
}

/// Returns true if R designates a static member function
static bool isStaticMemberFunction(const Reflection &R, APValue &Result) {
  if (const CXXMethodDecl *M = getAsMemberFunction(R))
    return SuccessBool(R, Result, M->isStatic());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a nonstatic member function
static bool isNonstaticMemberFunction(const Reflection &R, APValue &Result) {
  if (const CXXMethodDecl *M = getAsMemberFunction(R))
    return SuccessBool(R, Result, M->isInstance());
  return SuccessFalse(R, Result);
}

/// Returns the reflected data member.
static const FieldDecl *getAsDataMember(const Reflection &R) {
  if (const Decl *D = getReachableDecl(R))
    return dyn_cast<FieldDecl>(D);
  return nullptr;
}

/// Returns true if R designates a static member variable.
static bool isStaticDataMember(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R)) {
    if (const VarDecl *Var = dyn_cast<VarDecl>(D))
      return SuccessBool(R, Result, Var->isStaticDataMember());
  }
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a nonstatic data member.
static bool isNonstaticDataMember(const Reflection &R, APValue &Result) {
  if (const FieldDecl *D = getAsDataMember(R))
    // FIXME: Is a bitfield a non-static data member?
    return SuccessTrue(R, Result);
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a nonstatic data member.
static bool isBitField(const Reflection &R, APValue &Result) {
  if (const FieldDecl *D = getAsDataMember(R))
    return SuccessBool(R, Result, D->isBitField());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates an constructor.
static bool isConstructor(const Reflection &R,APValue &Result) {
  if (const Decl *D = getReachableDecl(R))
    return SuccessBool(R, Result, isa<CXXConstructorDecl>(D));
  return SuccessFalse(R, Result);
}

/// Returns true if R designates an enumerator.
static bool isDestructor(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R))
    return SuccessBool(R, Result, isa<CXXDestructorDecl>(D));
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a type.
static bool isType(const Reflection &R, APValue &Result) {
  if (MaybeType T = getReachableType(R))
    return SuccessTrue(R, Result);
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a function.
static bool isFunction(const Reflection &R, APValue &Result) {
  if (MaybeType T = getReachableCanonicalType(R)) {
    return SuccessBool(R, Result, T->isFunctionType());
  }
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a class.
static bool isClass(const Reflection &R, APValue &Result) {
  if (MaybeType T = getReachableCanonicalType(R)) {
    return SuccessBool(R, Result, T->isRecordType());
  }
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a union.
static bool isUnion(const Reflection &R, APValue &Result) {
  if (MaybeType T = getReachableCanonicalType(R))
    return SuccessBool(R, Result, T->isUnionType());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates an enum.
static bool isEnum(const Reflection &R, APValue &Result) {
  if (MaybeType T = getReachableCanonicalType(R))
    return SuccessBool(R, Result, T->isEnumeralType());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a scoped enum.
static bool isScopedEnum(const Reflection &R, APValue &Result) {
  if (MaybeType T = getReachableCanonicalType(R))
    return SuccessBool(R, Result, T->isScopedEnumeralType());
  return SuccessFalse(R, Result);
}

/// Returns true if R has void type.
static bool isVoid(const Reflection &R, APValue &Result) {
  if (MaybeType T = getReachableCanonicalType(R))
    return SuccessBool(R, Result, T->isVoidType());
  return SuccessFalse(R, Result);
}

/// Returns true if R has nullptr type.
static bool isNullPtr(const Reflection &R, APValue &Result) {
  if (MaybeType T = getReachableCanonicalType(R))
    return SuccessBool(R, Result, T->isNullPtrType());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates an type alias.
static bool isTypeAlias(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R))
    return SuccessBool(R, Result, isa<TypedefNameDecl>(D));
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a namespace.
static bool isNamespace(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R))
    return SuccessBool(R, Result, isa<NamespaceDecl>(D));
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a namespace alias.
static bool isNamespaceAlias(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R))
    return SuccessBool(R, Result, isa<NamespaceAliasDecl>(D));
  return SuccessFalse(R, Result);
}

/// Returns true if R designates an expression.
static bool isExpression(const Reflection &R, APValue &Result) {
  return SuccessBool(R, Result, R.isExpression());
}

bool Reflection::EvaluatePredicate(ReflectionQuery Q, APValue &Result) {
  assert(isPredicateQuery(Q) && "invalid query");
  switch (Q) {
  case RQ_is_invalid:
    return ::isInvalid(*this, Result);
  case RQ_is_entity:
    return isEntity(*this, Result);
  case RQ_is_unnamed:
    return isUnnamed(*this, Result);

  case RQ_is_variable:
    return isVariable(*this, Result);
  case RQ_is_enumerator:
    return isEnumerator(*this, Result);
  case RQ_is_static_data_member:
    return isStaticDataMember(*this, Result);
  case RQ_is_static_member_function:
    return isStaticMemberFunction(*this, Result);
  case RQ_is_nonstatic_data_member:
    return isNonstaticDataMember(*this, Result);
  case RQ_is_bitfield:
    return isBitField(*this, Result);
  case RQ_is_nonstatic_member_function:
    return isNonstaticMemberFunction(*this, Result);
  case RQ_is_constructor:
    return isConstructor(*this, Result);
  case RQ_is_destructor:
    return isDestructor(*this, Result);

  case RQ_is_type:
    return ::isType(*this, Result);
  case RQ_is_function:
    return isFunction(*this, Result);
  case RQ_is_class:
    return isClass(*this, Result);
  case RQ_is_union:
    return isUnion(*this, Result);
  case RQ_is_enum:
    return isEnum(*this, Result);
  case RQ_is_scoped_enum:
    return isScopedEnum(*this, Result);

  case RQ_is_void:
    return isVoid(*this, Result);
  case RQ_is_null_pointer:
    return isNullPtr(*this, Result);
  case RQ_is_integral:
  case RQ_is_floating_point:
  case RQ_is_array:
  case RQ_is_pointer:
  case RQ_is_lvalue_reference:
  case RQ_is_rvalue_reference:
  case RQ_is_member_object_pointer:
  case RQ_is_member_function_pointer:
  case RQ_is_closure:
    // FIXME: Implement these.
    return Error(*this);

  case RQ_is_type_alias:
    return isTypeAlias(*this, Result);

  case RQ_is_namespace:
    return isNamespace(*this, Result);
  case RQ_is_namespace_alias:
    return isNamespaceAlias(*this, Result);

  case RQ_is_template:
  case RQ_is_class_template:
  case RQ_is_alias_template:
  case RQ_is_function_template:
  case RQ_is_variable_template:
  case RQ_is_member_function_template:
  case RQ_is_static_member_function_template:
  case RQ_is_nonstatic_member_function_template:
  case RQ_is_constructor_template:
  case RQ_is_destructor_template:
  case RQ_is_concept:
  case RQ_is_specialization:
  case RQ_is_partial_specialization:
  case RQ_is_explicit_specialization:
  case RQ_is_implicit_instantiation:
  case RQ_is_explicit_instantiation:

  case RQ_is_direct_base:
  case RQ_is_virtual_base:

  case RQ_is_function_parameter:
  case RQ_is_template_parameter:
  case RQ_is_type_template_parameter:
  case RQ_is_nontype_template_parameter:
  case RQ_is_template_template_parameter:

  case RQ_is_expression:
    return ::isExpression(*this, Result);
  case RQ_is_lvalue:
  case RQ_is_xvalue:
  case RQ_is_rvalue:

  case RQ_is_local:
  case RQ_is_class_member:
    return Error(*this);

  default:
    break;
  }
  llvm_unreachable("invalid predicate selector");
}

/// Convert a bit-field structure into a uint32.
template <typename Traits>
static std::uint32_t TraitsToUnsignedInt(Traits S) {
  static_assert(sizeof(std::uint32_t) == sizeof(Traits), "size mismatch");
  unsigned ret = 0;
  std::memcpy(&ret, &S, sizeof(S));
  return ret;
}

template <typename Traits>
static APValue makeTraits(ASTContext *C, Traits S) {
  return APValue(C->MakeIntValue(TraitsToUnsignedInt(S), C->UnsignedIntTy));
}

template <typename Traits>
static bool SuccessTraits(const Reflection &R, Traits S, APValue &Result) {
  Result = makeTraits(R.Ctx, S);
  return true;
}

enum LinkageTrait : unsigned { LinkNone, LinkInternal, LinkExternal };

/// Remap linkage specifiers into a 2-bit value.
static LinkageTrait getLinkage(const NamedDecl *D) {
  switch (D->getFormalLinkage()) {
  case NoLinkage:
    return LinkNone;
  case InternalLinkage:
    return LinkInternal;
  case ExternalLinkage:
    return LinkExternal;
  default:
    break;
  }
  llvm_unreachable("Invalid linkage specification");
}

enum AccessTrait : unsigned {
  AccessNone,
  AccessPublic,
  AccessPrivate,
  AccessProtected
};

/// Returns the access specifiers for \p D.
static AccessTrait getAccess(const Decl *D) {
  switch (D->getAccess()) {
  case AS_public:
    return AccessPublic;
  case AS_private:
    return AccessPrivate;
  case AS_protected:
    return AccessProtected;
  case AS_none:
    return AccessNone;
  }
  llvm_unreachable("Invalid access specifier");
}

/// This gives the storage duration of declared objects, not the storage
/// specifier, which incorporates aspects of duration and linkage.
enum StorageTrait : unsigned {
  NoStorage,
  StaticStorage,
  AutomaticStorage,
  ThreadStorage,
};

/// Returns the storage duration of \p D.
static StorageTrait getStorage(const VarDecl *D) {
  switch (D->getStorageDuration()) {
  case SD_Automatic:
    return AutomaticStorage;
  case SD_Thread:
    return ThreadStorage;
  case SD_Static:
    return StaticStorage;
  default:
    break;
  }
  return NoStorage;
}

/// Traits for named objects.
///
/// Note that a variable can be declared \c extern and not be defined.
struct VariableTraits {
  LinkageTrait Linkage : 2;
  AccessTrait Access : 2;
  StorageTrait Storage : 2;
  unsigned Constexpr : 1;
  unsigned Defined : 1;
  unsigned Inline : 1; ///< Valid only when defined.
  unsigned Rest : 23;
};

static VariableTraits getVariableTraits(const VarDecl *D) {
  VariableTraits T = VariableTraits();
  T.Linkage = getLinkage(D);
  T.Access = getAccess(D);
  T.Storage = getStorage(D);
  T.Constexpr = D->isConstexpr();
  T.Defined = D->getDefinition() != nullptr;
  T.Inline = D->isInline();
  return T;
}

/// Traits for named sub-objects of a class (or union?).
struct FieldTraits {
  LinkageTrait Linkage : 2;
  AccessTrait Access : 2;
  unsigned Mutable : 1;
  unsigned Rest : 27;
};

/// Get the traits for a non-static member of a class or union.
static FieldTraits getFieldTraits(const FieldDecl *D) {
  FieldTraits T = FieldTraits();
  T.Linkage = getLinkage(D);
  T.Access = getAccess(D);
  T.Mutable = D->isMutable();
  return T;
}

/// Computed traits of normal, extern local, and static class functions.
///
// TODO: Add calling conventions to function traits.
struct FunctionTraits {
  LinkageTrait Linkage : 2;
  AccessTrait Access : 2;
  unsigned Constexpr : 1;
  unsigned Nothrow : 1; ///< Called \c noexcept in C++.
  unsigned Defined : 1;
  unsigned Inline : 1;  ///< Valid only when defined.
  unsigned Deleted : 1; ///< Valid only when defined.
  unsigned Rest : 23;
};

static bool getNothrow(const FunctionDecl *D) {
  if (const FunctionProtoType *Ty = D->getType()->getAs<FunctionProtoType>())
    return Ty->isNothrow();
  return false;
}

static FunctionTraits getFunctionTraits(const FunctionDecl *D) {
  FunctionTraits T = FunctionTraits();
  T.Linkage = getLinkage(D);
  T.Access = getAccess(D);
  T.Constexpr = D->isConstexpr();
  T.Nothrow = getNothrow(D);
  T.Defined = D->getDefinition() != nullptr;
  T.Inline = D->isInlined();
  T.Deleted = D->isDeleted();
  return T;
}

enum MethodKind : unsigned {
  Method,
  Constructor,
  Destructor,
  Conversion
};

/// Traits for normal member functions.
struct MethodTraits {
  LinkageTrait Linkage : 2;
  AccessTrait Access : 2;
  MethodKind Kind : 2;
  unsigned Constexpr : 1;
  unsigned Explicit : 1;
  unsigned Virtual : 1;
  unsigned Pure : 1;
  unsigned Final : 1;
  unsigned Override : 1;
  unsigned Nothrow : 1; ///< Called \c noexcept in C++.
  unsigned Defined : 1;
  unsigned Inline : 1;
  unsigned Deleted : 1;
  unsigned Defaulted : 1;
  unsigned Trivial : 1;
  unsigned DefaultCtor : 1;
  unsigned CopyCtor : 1;
  unsigned MoveCtor : 1;
  unsigned CopyAssign : 1;
  unsigned MoveAssign : 1;
  unsigned Rest : 9;
};

static MethodTraits getMethodTraits(const CXXConstructorDecl *D) {
  MethodTraits T = MethodTraits();
  T.Linkage = getLinkage(D);
  T.Access = getAccess(D);
  T.Kind = Constructor;
  T.Constexpr = D->isConstexpr();
  T.Nothrow = getNothrow(D);
  T.Defined = D->getDefinition() != nullptr;
  T.Inline = D->isInlined();
  T.Deleted = D->isDeleted();
  T.Defaulted = D->isDefaulted();
  T.Trivial = D->isTrivial();
  T.DefaultCtor = D->isDefaultConstructor();
  T.CopyCtor = D->isCopyConstructor();
  T.MoveCtor = D->isMoveConstructor();
  return T;
}

static MethodTraits getMethodTraits(const CXXDestructorDecl *D) {
  MethodTraits T = MethodTraits();
  T.Linkage = getLinkage(D);
  T.Access = getAccess(D);
  T.Kind = Destructor;
  T.Virtual = D->isVirtual();
  T.Pure = D->isPure();
  T.Final = D->hasAttr<FinalAttr>();
  T.Override = D->hasAttr<OverrideAttr>();
  T.Nothrow = getNothrow(D);
  T.Defined = D->getDefinition() != nullptr;
  T.Inline = D->isInlined();
  T.Deleted = D->isDeleted();
  T.Defaulted = D->isDefaulted();
  T.Trivial = D->isTrivial();
  return T;
}

static MethodTraits getMethodTraits(const CXXConversionDecl *D) {
  MethodTraits T = MethodTraits();
  T.Linkage = getLinkage(D);
  T.Access = getAccess(D);
  T.Kind = Conversion;
  T.Constexpr = D->isConstexpr();
  T.Explicit = D->isExplicit();
  T.Virtual = D->isVirtual();
  T.Pure = D->isPure();
  T.Final = D->hasAttr<FinalAttr>();
  T.Override = D->hasAttr<OverrideAttr>();
  T.Nothrow = getNothrow(D);
  T.Defined = D->getDefinition() != nullptr;
  T.Inline = D->isInlined();
  T.Deleted = D->isDeleted();
  return T;
}

static MethodTraits getMethodTraits(const CXXMethodDecl *D) {
  MethodTraits T = MethodTraits();
  T.Linkage = getLinkage(D);
  T.Access = getAccess(D);
  T.Kind = Method;
  T.Constexpr = D->isConstexpr();
  T.Virtual = D->isVirtual();
  T.Pure = D->isPure();
  T.Final = D->hasAttr<FinalAttr>();
  T.Override = D->hasAttr<OverrideAttr>();
  T.Nothrow = getNothrow(D);
  T.Defined = D->getDefinition() != nullptr;
  T.Inline = D->isInlined();
  T.Deleted = D->isDeleted();
  T.CopyAssign = D->isCopyAssignmentOperator();
  T.MoveAssign = D->isMoveAssignmentOperator();
  return T;
}

struct ValueTraits {
  LinkageTrait Linkage : 2;
  AccessTrait Access : 2;
  unsigned Rest : 28;
};

static ValueTraits getValueTraits(const EnumConstantDecl *D) {
  ValueTraits T = ValueTraits();
  T.Linkage = getLinkage(D);
  T.Access = getAccess(D);
  return T;
}

struct NamespaceTraits {
  LinkageTrait Linkage : 2;
  AccessTrait Access : 2;
  bool Inline : 1;
  unsigned Rest : 27;
};

static NamespaceTraits getNamespaceTraits(const NamespaceDecl *D) {
  NamespaceTraits T = NamespaceTraits();
  T.Linkage = getLinkage(D);
  T.Access = getAccess(D);
  T.Inline = D->isInline();
  return T;
}

static bool makeDeclTraits(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R)) {
    if (const VarDecl *Var = dyn_cast<VarDecl>(D))
      return SuccessTraits(R, getVariableTraits(Var), Result);
    else if (const FieldDecl *Field = dyn_cast<FieldDecl>(D))
      return SuccessTraits(R, getFieldTraits(Field), Result);
    else if (const CXXConstructorDecl *Ctor = dyn_cast<CXXConstructorDecl>(D))
      return SuccessTraits(R, getMethodTraits(Ctor), Result);
    else if (const CXXDestructorDecl *Dtor = dyn_cast<CXXDestructorDecl>(D))
      return SuccessTraits(R, getMethodTraits(Dtor), Result);
    else if (const CXXConversionDecl *Conv = dyn_cast<CXXConversionDecl>(D))
      return SuccessTraits(R, getMethodTraits(Conv), Result);
    else if (const CXXMethodDecl *Meth = dyn_cast<CXXMethodDecl>(D))
      return SuccessTraits(R, getMethodTraits(Meth), Result);
    else if (const FunctionDecl *Fn = dyn_cast<FunctionDecl>(D))
      return SuccessTraits(R, getFunctionTraits(Fn), Result);
    else if (const EnumConstantDecl *Enum = dyn_cast<EnumConstantDecl>(D))
      return SuccessTraits(R, getValueTraits(Enum), Result);
    else if (const NamespaceDecl *Ns = dyn_cast<NamespaceDecl>(D))
      return SuccessTraits(R, getNamespaceTraits(Ns), Result);
  }

  return Error(R);
}

bool Reflection::GetTraits(ReflectionQuery Q, APValue &Result) {
  assert(isTraitQuery(Q) && "invalid query");
  switch (Q) {
  // Traits
  case RQ_get_decl_traits:
    return makeDeclTraits(*this, Result);
  case RQ_get_linkage_traits:
  case RQ_get_access_traits:
    return Error(*this);

  default:
    break;
  }
  llvm_unreachable("invalid traits selector");
}

/// Set Result to an invalid reflection.
static bool makeReflection(APValue &Result) {
  Result = APValue(RK_invalid, nullptr);
  return true;
}

/// Set Result to a reflection of D.
static bool makeReflection(const Decl *D, APValue &Result) {
  if (!D)
    return makeReflection(Result);
  Result = APValue(RK_declaration, D);
  return true;
}

/// Set Result to a reflection of D.
static bool makeReflection(const DeclContext *DC, APValue &Result) {
  if (!DC)
    return makeReflection(Result);
  Result = APValue(RK_declaration, Decl::castFromDeclContext(DC));
  return true;
}

/// Set Result to a reflection of T.
static bool makeReflection(QualType T, APValue &Result) {
  if (T.isNull())
    return makeReflection(Result);
  Result = APValue(RK_type, T.getAsOpaquePtr());
  return true;
}

/// Set Result to a reflection of E.
static bool makeReflection(const Expr *E, APValue &Result) {
  Result = APValue(RK_declaration, E);
  return true;
}

/// Set Result to a reflection of B.
static bool makeReflection(const CXXBaseSpecifier *B, APValue &Result) {
  Result = APValue(RK_declaration, B);
  return true;
}

static bool getEntity(const Reflection &R, APValue &Result) {
  if (R.isType()) {
    /// The entity is the canonical type.
    QualType T = R.Ctx->getCanonicalType(R.getAsType());
    return makeReflection(T, Result);
  }
  if (R.isDeclaration()) {
    /// The entity is the canonical declaration.
    const Decl *D = R.getAsDeclaration()->getCanonicalDecl();
    return makeReflection(D, Result);
  }
  if (R.isExpression()) {
    /// The entity is the reachable declaration.
    if (const Decl *D = getReachableDecl(R))
      return makeReflection(D, Result);

    // FIXME: Give a better error message.
    return Error(R);
  }
  if (R.isBase()) {
    // The entity is the canonical type named by the specifier.
    const CXXBaseSpecifier *Base = R.getAsBase();
    QualType T = R.Ctx->getCanonicalType(Base->getType());
    return makeReflection(T, Result);
  }
  return Error(R);
}

static bool getParent(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R))
    return makeReflection(D->getDeclContext(), Result);
  return Error(R);
}

static bool getType(const Reflection &R, APValue &Result) {
  if (const ValueDecl *VD = getReachableValueDecl(R))
    return makeReflection(VD->getType(), Result);

  // FIXME: Emit an appropriate error diagnostic.
  return Error(R);
}

/// True if D is reflectable. Some declarations are not reflected (e.g.,
/// access specifiers).
static bool isReflectableDecl(const Decl *D) {
  assert(D && "null declaration");
  if (isa<AccessSpecDecl>(D))
    return false;
  if (const CXXRecordDecl *Class = dyn_cast<CXXRecordDecl>(D))
    if (Class->isInjectedClassName())
      return false;
  return true;
}

/// Filter non-reflectable members.
static const Decl *findNextMember(const Decl *D) {
  while (D && !isReflectableDecl(D))
    D = D->getNextDeclInContext();
  return D;
}

/// Returns the first reflectable member.
static const Decl *getFirstMember(const DeclContext *DC) {
  return findNextMember(*DC->decls_begin());
}

/// Returns the next reflectable member
static const Decl *getNextMember(const Decl *D) {
  return findNextMember(D->getNextDeclInContext());
}

/// Returns the reachable declaration context for R, if any.
static const DeclContext *getReachableDeclContext(const Reflection &R) {
  if (const Decl *D = getReachableDecl(R))
    if (const DeclContext *DC = dyn_cast<DeclContext>(D))
      return DC;
  return nullptr;
}

/// Returns the first member
static bool getBegin(const Reflection &R, APValue &Result) {
  if (const DeclContext *DC = getReachableDeclContext(R))
    return makeReflection(getFirstMember(DC), Result);

  // FIXME: Emit an appropriate error diagnostic.
  return Error(R);
}

static bool getNext(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R))
    return makeReflection(getNextMember(D), Result);

  // FIXME: Emit an appropriate error diagnostic.
  return Error(R);
}


bool Reflection::GetAssociatedReflection(ReflectionQuery Q, APValue &Result) {
  assert(isAssociatedReflectionQuery(Q) && "invalid query");
  switch (Q) {
  // Associated reflections
  case RQ_get_entity:
    return getEntity(*this, Result);
  case RQ_get_parent:
    return getParent(*this, Result);
  case RQ_get_type:
    return getType(*this, Result);
  case RQ_get_this_ref_type:
    return Error(*this);

  // Traversal
  case RQ_get_begin:
    return getBegin(*this, Result);
  case RQ_get_next:
    return getNext(*this, Result);

  default:
    break;
  }
  llvm_unreachable("invalid reflection selector");
}

static StringLiteral *makeString(const Reflection &R, StringRef Str) {
  QualType StrTy = R.Ctx->getConstantArrayType(R.Ctx->CharTy.withConst(), 
                                               llvm::APInt(32, Str.size() + 1), 
                                               ArrayType::Normal, 0);
  return StringLiteral::Create(*R.Ctx, Str, StringLiteral::Ascii, false, 
                               StrTy, SourceLocation());
}

bool getName(const Reflection R, APValue &Result) {
  if (R.isType()) {
    QualType T = R.getAsType();
    
    // See through loc infos.
    if (const LocInfoType *LIT = dyn_cast<LocInfoType>(T))
      T = LIT->getType();

    // Render the string of the type.
    PrintingPolicy PP = R.Ctx->getPrintingPolicy();
    PP.SuppressTagKeyword = true;
    StringLiteral *Str = makeString(R, T.getAsString(PP));

    // Generate the result value.
    Expr::EvalResult Eval;
    if (!Str->EvaluateAsLValue(Eval, *R.Ctx))
      return false;
    Result = Eval.Val;
    return true;
  }

  if (const NamedDecl *ND = dyn_cast<NamedDecl>(getReachableDecl(R))) {
    if (IdentifierInfo *II = ND->getIdentifier()) {
      // Get the identifier of the declaration.
      StringLiteral *Str = makeString(R, II->getName());
      
      // Generate the result value.
      Expr::EvalResult Eval;
      if (!Str->EvaluateAsLValue(Eval, *R.Ctx))
        return false;
      Result = Eval.Val;
      return true;
    }
  }

  return Error(R);
}

bool Reflection::GetName(ReflectionQuery Q, APValue &Result) {
  assert(isNameQuery(Q) && "invalid query");
  switch (Q) {
  // Names
  case RQ_get_name:
  case RQ_get_display_name:
    return getName(*this, Result);

  default:
    break;
  }

  llvm_unreachable("invalid name selector");
}

/// Returns true if canonical types are equal.
static bool EqualTypes(ASTContext &Ctx, QualType A, QualType B) {
  CanQualType CanA = Ctx.getCanonicalType(A);
  CanQualType CanB = Ctx.getCanonicalType(B);
  return CanA == CanB;
}

/// Returns true if the declared entities are the same.
static bool EqualDecls(const Decl *A, const Decl *B) {
  const Decl *CanA = A->getCanonicalDecl();
  const Decl *CanB = B->getCanonicalDecl();
  return CanA == CanB;
}

bool Reflection::Equal(ASTContext &Ctx, APValue const& A, APValue const& B) {
  assert(A.isReflection() && B.isReflection());

  if (A.getReflectionKind() != B.getReflectionKind())
    return false;

  switch (A.getReflectionKind()) {
  case RK_invalid:
    return true;
  case RK_type:
    return EqualTypes(Ctx, 
                      A.getReflectedType(), 
                      B.getReflectedType());
  case RK_declaration:
    return EqualDecls(A.getReflectedDeclaration(), 
                      B.getReflectedDeclaration());
  default:
    return false;
  }
}
