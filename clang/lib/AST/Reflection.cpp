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

#include "clang/AST/Attr.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTDiagnostic.h"
#include "clang/AST/Reflection.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/LocInfoType.h"
#include "clang/Sema/Sema.h"
#include "clang/Sema/SemaDiagnostic.h"

namespace clang {
  std::string ReflectionModifiers::getNewNameAsString() const {
    return cast<StringLiteral>(NewName)->getString().str();
  }

  enum ReflectionQuery : unsigned {
    query_unknown,

    query_is_invalid,
    query_is_entity,
    query_is_named,

    // Scopes
    query_is_local,

    // Variables
    query_is_variable,
    query_has_static_storage,
    query_has_thread_local_storage,
    query_has_automatic_local_storage,

    // Functions
    query_is_function,
    query_is_nothrow,
    // query_has_ellipsis,

    // Classes
    query_is_class,
    query_is_union,
    query_has_virtual_destructor,
    query_is_declared_struct,
    query_is_declared_class,

    // Class members
    query_is_class_member,

    // Data members
    query_is_static_data_member,
    query_is_nonstatic_data_member,
    query_is_bit_field,
    query_is_mutable,

    // Member functions
    query_is_static_member_function,
    query_is_nonstatic_member_function,
    query_is_normal,
    query_is_override,
    query_is_override_specified,
    query_is_deleted,
    query_is_virtual,
    query_is_pure_virtual,

    // Special members
    query_is_constructor,
    query_is_default_constructor,
    query_is_copy_constructor,
    query_is_move_constructor,
    query_is_copy_assignment_operator,
    query_is_move_assignment_operator,
    query_is_destructor,
    query_is_conversion,
    query_is_defaulted,
    query_is_explicit,

    // Access
    query_has_access,
    query_is_public,
    query_is_protected,
    query_is_private,
    query_has_default_access,

    // Linkage
    query_has_linkage,
    query_is_externally_linked,
    query_is_internally_linked,

    // Initializers
    query_has_initializer,

    // General purpose
    query_is_extern_specified,
    query_is_inline,
    query_is_inline_specified,
    query_is_constexpr,
    query_is_consteval,
    query_is_final,
    query_is_defined,
    query_is_complete,

    // Namespaces
    query_is_namespace,

    // Aliases
    query_is_namespace_alias,
    query_is_type_alias,
    query_is_alias_template,

    // Enums
    query_is_unscoped_enum,
    query_is_scoped_enum,

    // Enumerators
    query_is_enumerator,

    // Templates
    query_is_template,
    query_is_class_template,
    query_is_function_template,
    query_is_variable_template,
    query_is_static_member_function_template,
    query_is_nonstatic_member_function_template,
    query_is_constructor_template,
    query_is_destructor_template,
    query_is_concept,

    // Specializations
    query_is_specialization,
    query_is_partial_specialization,
    query_is_explicit_specialization,
    query_is_implicit_instantiation,
    query_is_explicit_instantiation,

    // Base class specifiers
    query_is_direct_base,
    query_is_virtual_base,

    // Parameters
    query_is_function_parameter,
    query_is_type_template_parameter,
    query_is_nontype_template_parameter,
    query_is_template_template_parameter,
    query_has_default_argument,

    // Attributes
    query_has_attribute,

    // Types
    query_is_type,
    query_is_fundamental_type,
    query_is_arithmetic_type,
    query_is_scalar_type,
    query_is_object_type,
    query_is_compound_type,
    query_is_function_type,
    query_is_class_type,
    query_is_union_type,
    query_is_unscoped_enum_type,
    query_is_scoped_enum_type,
    query_is_void_type,
    query_is_null_pointer_type,
    query_is_integral_type,
    query_is_floating_point_type,
    query_is_array_type,
    query_is_pointer_type,
    query_is_lvalue_reference_type,
    query_is_rvalue_reference_type,
    query_is_member_object_pointer_type,
    query_is_member_function_pointer_type,
    query_is_closure_type,

    // Type properties
    query_is_incomplete_type,
    query_is_const_type,
    query_is_volatile_type,
    query_is_trivial_type,
    query_is_trivially_copyable_type,
    query_is_standard_layout_type,
    query_is_pod_type,
    query_is_literal_type,
    query_is_empty_type,
    query_is_polymorphic_type,
    query_is_abstract_type,
    query_is_final_type,
    query_is_aggregate_type,
    query_is_signed_type,
    query_is_unsigned_type,
    query_has_unique_object_representations_type,

    // Type operations
    query_is_constructible,
    query_is_trivially_constructible,
    query_is_nothrow_constructible,
    query_is_assignable,
    query_is_trivially_assignable,
    query_is_nothrow_assignable,
    query_is_destructible,
    query_is_trivially_destructible,
    query_is_nothrow_destructible,

    // Captures
    query_has_default_ref_capture,
    query_has_default_copy_capture,
    query_is_capture,
    query_is_simple_capture,
    query_is_ref_capture,
    query_is_copy_capture,
    query_is_explicit_capture,
    query_is_init_capture,
    query_has_captures,

    // Expressions
    query_is_expression,
    query_is_lvalue,
    query_is_xvalue,
    query_is_prvalue,
    query_is_value,

    // Associated types
    query_get_type,
    query_get_return_type,
    query_get_this_ref_type,
    query_get_underlying_type,

    // Entities
    query_get_entity,
    query_get_parent,
    query_get_definition,

    // Traversal
    query_get_begin,
    query_get_next,
    query_get_begin_template_param,
    query_get_next_template_param,
    query_get_begin_param,
    query_get_next_param,
    query_get_begin_member,
    query_get_next_member,
    query_get_begin_base_spec,
    query_get_next_base_spec,

    // Type transformations
    query_remove_const,
    query_remove_volatile,
    query_add_const,
    query_add_volatile,
    query_remove_reference,
    query_add_lvalue_reference,
    query_add_rvalue_reference,
    query_remove_extent,
    query_remove_pointer,
    query_add_pointer,
    query_make_signed,
    query_make_unsigned,

    // Names
    query_get_name,
    query_get_display_name,

    // Modifier updates
    query_set_access,
    query_set_storage,
    query_set_constexpr,
    query_set_add_explicit,
    query_set_add_virtual,
    query_set_add_pure_virtual,
    query_set_add_inline,
    query_set_new_name,

    // Labels for kinds of queries. These need to be updated when new
    // queries are added.

    // Predicates -- these return bool.
    query_first_predicate = query_is_invalid,
    query_last_predicate = query_is_value,
    // Associated reflections -- these return meta::info.
    query_first_assoc = query_get_type,
    query_last_assoc = query_make_unsigned,
    // Names -- these return const char*
    query_first_name = query_get_name,
    query_last_name = query_get_display_name,
    // Modifier updates -- these return meta::info.
    query_first_modifier_update = query_set_access,
    query_last_modifier_update = query_set_new_name,

    // Type operations -- these require callbacks to be present.
    query_first_type_operation = query_is_constructible,
    query_last_type_operation = query_is_nothrow_constructible
  };

  ReflectionQuery getUnknownReflectionQuery() {
    return query_unknown;
  }

  unsigned getMinNumQueryArguments(ReflectionQuery Q) {
    if (isPredicateQuery(Q)) {
      switch (Q) {
      case query_is_constructible:
      case query_is_trivially_constructible:
      case query_is_nothrow_constructible:
        return 0;
      case query_has_attribute:
      case query_is_assignable:
      case query_is_trivially_assignable:
      case query_is_nothrow_assignable:
        return 2;
      default:
        return 1;
      }
    }

    if (isModifierUpdateQuery(Q)) {
      return 2;
    }

    return 1;
  }

  unsigned getMaxNumQueryArguments(ReflectionQuery Q) {
    if (isPredicateQuery(Q)) {
      switch (Q) {
      case query_is_constructible:
      case query_is_trivially_constructible:
      case query_is_nothrow_constructible:
        return UINT_MAX - 1;
      case query_has_attribute:
      case query_is_assignable:
      case query_is_trivially_assignable:
      case query_is_nothrow_assignable:
        return 2;
      default:
        return 1;
      }
    }

    if (isModifierUpdateQuery(Q)) {
      return 2;
    }

    return 1;
  }

  bool isPredicateQuery(ReflectionQuery Q) {
    return query_first_predicate <= Q && Q <= query_last_predicate;
  }

  bool isAssociatedReflectionQuery(ReflectionQuery Q) {
    return query_first_assoc <= Q && Q <= query_last_assoc;
  }

  bool isNameQuery(ReflectionQuery Q) {
    return query_first_name <= Q && Q <= query_last_name;
  }

  bool isModifierUpdateQuery(ReflectionQuery Q) {
    return query_first_modifier_update <= Q && Q <= query_last_modifier_update;
  }
}

using namespace clang;

/// Returns an APValue-packaged truth value.
static APValue makeBool(ASTContext &C, bool B) {
  return APValue(C.MakeIntValue(B, C.BoolTy));
}

/// Sets result to the truth value of B and returns true.
static bool SuccessBool(const Reflection &R, APValue &Result, bool B) {
  Result = makeBool(R.getContext(), B);
  return true;
}

/// Sets result to the truth value of B and returns true.
static bool SuccessBool(const ReflectionQueryEvaluator &Eval,
                        APValue &Result, bool B) {
  Result = makeBool(Eval.getContext(), B);
  return true;
}

static bool SuccessTrue(const Reflection &R, APValue &Result) {
  return SuccessBool(R, Result, true);
}

static bool SuccessTrue(const ReflectionQueryEvaluator &Eval,
                        APValue &Result) {
  return SuccessBool(Eval, Result, true);
}

static bool SuccessFalse(const Reflection &R, APValue &Result) {
  return SuccessBool(R, Result, false);
}

static bool SuccessFalse(const ReflectionQueryEvaluator &Eval,
                        APValue &Result) {
  return SuccessBool(Eval, Result, false);
}

template<typename F>
static bool CustomError(const Reflection &R, F BuildDiagnostic) {
  if (SmallVectorImpl<PartialDiagnosticAt> *Diag = R.getDiag()) {
    // FIXME: We could probably do a better job with the location.
    SourceLocation Loc = R.getQuery()->getExprLoc();
    Diag->push_back(std::make_pair(Loc, BuildDiagnostic()));
  }
  return false;
}

/// Returns the type reflected by R. R must be a type reflection.
///
/// Note that this does not get the canonical type.
static QualType getQualType(QualType QT) {
  // See through "location types".
  if (const LocInfoType *LIT = dyn_cast<LocInfoType>(QT))
    return LIT->getType();

  return QT;
}

static QualType getQualType(const Reflection &R) {
  return getQualType(R.getAsType());
}

static QualType getQualType(const APValue &R) {
  return getQualType(R.getReflectedType());
}

// Returns false, possibly saving the diagnostic.
static bool Error(const Reflection &R) {
  return CustomError(R, [&]() {
    PartialDiagnostic PD(diag::note_reflection_not_defined,
                         R.getContext().getDiagAllocator());

    switch (R.getKind()) {
    case RK_type:
      PD << 1;
      PD << getQualType(R);
      break;

    default:
      PD << 0;
      break;
    }

    return PD;
  });
}

static bool ErrorUnimplemented(const Reflection &R) {
  return CustomError(R, [&]() {
    return PartialDiagnostic(diag::note_reflection_query_unimplemented,
                             R.getContext().getDiagAllocator());
  });
}

/// Returns the TypeDecl for a qualified type, if any.
static const TypeDecl *getAsTypeAliasDecl(QualType QT) {
  const Type *T = QT.getTypePtr();

  if (const ElaboratedType *ET = dyn_cast<ElaboratedType>(T))
    T = &(*ET->desugar());

  if (const TypedefType *TDT = dyn_cast<TypedefType>(T))
    return TDT->getDecl();

  if (const TagDecl *TD = T->getAsTagDecl())
    return TD;

  return nullptr;
}

/// Returns the TypeDecl for a reflected Type, if any.
static const TypeDecl *getAsTypeAliasDecl(const Reflection &R) {
  if (R.isType())
    return getAsTypeAliasDecl(getQualType(R));
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
static const Decl *getReachableAliasDecl(const Reflection &R) {
  if (const TypeDecl *TD = getAsTypeAliasDecl(R))
    return TD;
  if (R.isDeclaration())
    return R.getAsDeclaration();
  if (R.isExpression())
    return getEntityDecl(R.getAsExpression());
  if (R.isBase())
    return getAsTypeAliasDecl(R.getAsBase()->getType());
  return nullptr;
}

/// If R designates some kind of declaration, either directly, as a type,
/// or via a reflection, return that declaration.
///
/// For types this method considers only the canonical non-alias type.
static const Decl *getReachableDecl(const Reflection &R) {
  if (const Decl *D = getReachableAliasDecl(R)) {
    if (const auto *TDND = dyn_cast<TypedefNameDecl>(D)) {
      QualType QT = TDND->getUnderlyingType();
      return QT->getAsTagDecl();
    }

    return D;
  }
  return nullptr;
}

/// If R designates a base, return the base.
static const CXXBaseSpecifier *getReachableBase(const Reflection &R) {
  if (R.isBase())
    return R.getAsBase();
  return nullptr;
}

namespace {

/// A helper class to manage conditions involving types.
struct MaybeType {
  MaybeType(QualType T) : Ty(T) { }

  explicit operator bool() const { return !Ty.isNull(); }

  operator QualType() const {
    assert(!Ty.isNull());
    return Ty;
  }

  const Type *getTypePtr() const { return Ty.getTypePtr(); }

  const Type* operator->() const { return Ty.getTypePtr(); }

  QualType operator*() const { return Ty; }

  QualType Ty;
};

} // end anonymous namespace


/// Returns the canonical type reflected by R, if R is a type reflection.
///
/// This is used for queries concerned with type entities
/// rather than e.g., aliases.
static QualType getCanonicalType(const Reflection &R) {
  if (R.isType()) {
    return R.getContext().getCanonicalType(getQualType(R));
  }

  return QualType();
}

static const Expr *getExpr(const Reflection &R) {
  if (R.isExpression())
    return R.getAsExpression();
  return nullptr;
}

const Decl *Reflection::getAsReachableDeclaration() const {
  return getReachableAliasDecl(*this);
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

// Returns a named declaration which is not required to
// be the canonical declaration of the named entity.
//
// If there is no named reachable named declaration, returns null.
static const NamedDecl *getReachableNamedAliasDecl(const Reflection &R) {
  if (const Decl *D = getReachableAliasDecl(R))
    return dyn_cast<NamedDecl>(D);

  return nullptr;
}

/// Returns true if R is named.
static bool isNamed(const Reflection &R, APValue &Result) {
  if (R.isType())
    return SuccessTrue(R, Result);

  if (getReachableNamedAliasDecl(R))
    return SuccessTrue(R, Result);

  return SuccessFalse(R, Result);
}

/// Scope

static const DeclContext *getReachableRedeclContext(const Reflection &R) {
  if (const Decl *D = getReachableAliasDecl(R))
    if (const DeclContext *DC = D->getLexicalDeclContext())
      return DC->getRedeclContext();
  return nullptr;
}

/// Returns true if R designates a local entity.
static bool isLocal(const Reflection &R, APValue &Result) {
  if (const DeclContext *DC = getReachableRedeclContext(R))
    return SuccessBool(R, Result, DC->isFunctionOrMethod());
  return SuccessFalse(R, Result);
}

static const VarDecl *getAsVarDecl(const Reflection &R) {
  if (const Decl *D = getReachableDecl(R))
    return dyn_cast<VarDecl>(D);
  return nullptr;
}

/// Returns true if R designates a variable.
static bool isVariable(const Reflection &R, APValue &Result) {
  return SuccessBool(R, Result, getAsVarDecl(R));
}

static bool hasStaticStorage(const Reflection &R, APValue &Result) {
  if (const VarDecl *D = getAsVarDecl(R))
    return SuccessBool(R, Result, D->getStorageDuration() == SD_Static);
  return SuccessFalse(R, Result);
}

static bool hasThreadLocalStorage(const Reflection &R, APValue &Result) {
  if (const VarDecl *D = getAsVarDecl(R))
    return SuccessBool(R, Result, D->getStorageDuration() == SD_Thread);
  return SuccessFalse(R, Result);
}

static bool hasAutomaticLocalStorage(const Reflection &R, APValue &Result) {
  if (const VarDecl *D = getAsVarDecl(R))
    return SuccessBool(R, Result, D->getStorageDuration() == SD_Automatic);
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a function.
static bool isFunction(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R))
    return SuccessBool(R, Result, isa<FunctionDecl>(D));
  if (MaybeType T = getCanonicalType(R))
    return SuccessBool(R, Result, isa<FunctionType>(T.getTypePtr()));
  return SuccessFalse(R, Result);
}

static bool isNothrow(const Reflection &R, const QualType T, APValue &Result) {
  if (const FunctionProtoType *Proto = T->getAs<FunctionProtoType>())
    return SuccessBool(R, Result, Proto->hasNoexceptExceptionSpec());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a function which does not throw.
static bool isNothrow(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R)) {
    if (const FunctionDecl *FD = dyn_cast<FunctionDecl>(D))
      return isNothrow(R, FD->getType(), Result);
  }

  if (MaybeType T = getCanonicalType(R))
    return isNothrow(R, T, Result);

  return SuccessFalse(R, Result);
}

/// Classes

static const CXXRecordDecl *getReachableRecordDecl(const Reflection &R) {
  if (const Decl *D = getReachableDecl(R))
    return dyn_cast<CXXRecordDecl>(D);
  return nullptr;
}

static const CXXRecordDecl *getReachableClassDecl(const Reflection &R) {
  if (const CXXRecordDecl *D = getReachableRecordDecl(R)) {
    if (D->isClass() || D->isStruct())
      return D;
  }
  return nullptr;
}

/// Returns true if R designates a class.
static bool isClass(const Reflection &R, APValue &Result) {
  return SuccessBool(R, Result, getReachableClassDecl(R));
}

/// Returns true if R designates a union.
static bool isUnion(const Reflection &R, APValue &Result) {
  if (const CXXRecordDecl *D = getReachableRecordDecl(R))
    return SuccessBool(R, Result, D->isUnion());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a class with a virtual destructor.
static bool hasVirtualDestructor(const Reflection &R, APValue &Result) {
  if (const CXXRecordDecl *D = getReachableClassDecl(R)) {
    if (const CXXDestructorDecl *DD = D->getDestructor())
      return SuccessBool(R, Result, DD->isVirtual());
  }
  return SuccessFalse(R, Result);
}

/// Returns true if Args[1] designates a class declared as a struct.
static bool isDeclaredStruct(ReflectionQueryEvaluator &Eval,
                             SmallVectorImpl<APValue> &Args,
                             APValue &Result) {
  Reflection R(Eval.getContext(), Args[1]);
  if (const CXXRecordDecl *D = getReachableClassDecl(R)) {
    return SuccessBool(Eval, Result, D->isStruct());
  }
  return SuccessFalse(Eval, Result);
}

/// Returns true if Args[1] designates a class declared as a class.
static bool isDeclaredClass(ReflectionQueryEvaluator &Eval,
                            SmallVectorImpl<APValue> &Args,
                            APValue &Result) {
  Reflection R(Eval.getContext(), Args[1]);
  if (const CXXRecordDecl *D = getReachableClassDecl(R)) {
    return SuccessBool(Eval, Result, D->isClass());
  }
  return SuccessFalse(Eval, Result);
}

/// Class members

/// Returns true if R designates a class member.
static bool isClassMember(const Reflection &R, APValue &Result) {
  if (const DeclContext *DC = getReachableRedeclContext(R))
    return SuccessBool(R, Result, DC->isRecord());
  return SuccessFalse(R, Result);
}

/// Data members

/// Returns the reflected data member.
static const FieldDecl *getAsDataMember(const Reflection &R) {
  return dyn_cast_or_null<FieldDecl>(getReachableDecl(R));
}

/// Returns true if R designates a static member variable.
static bool isStaticDataMember(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R))
    if (const VarDecl *Var = dyn_cast<VarDecl>(D))
      return SuccessBool(R, Result, Var->isStaticDataMember());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a nonstatic data member.
static bool isNonstaticDataMember(const Reflection &R, APValue &Result) {
  if (getAsDataMember(R))
    return SuccessTrue(R, Result);
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a nonstatic data member.
static bool isBitField(const Reflection &R, APValue &Result) {
  if (const FieldDecl *D = getAsDataMember(R))
    return SuccessBool(R, Result, D->isBitField());
  return SuccessFalse(R, Result);
}

/// Returns true if Args[1] designates a mutable data member.
static bool isMutable(ReflectionQueryEvaluator &Eval,
                      SmallVectorImpl<APValue> &Args,
                      APValue &Result) {
  Reflection R(Eval.getContext(), Args[1]);
  if (const FieldDecl *D = getAsDataMember(R))
    return SuccessBool(Eval, Result, D->isMutable());
  return SuccessFalse(Eval, Result);
}

/// Returns the reflected member function.
static const CXXMethodDecl *getAsMemberFunction(const Reflection &R) {
  return dyn_cast_or_null<CXXMethodDecl>(getReachableDecl(R));
}

/// Returns the reflected constructor.
static const CXXConstructorDecl *getReachableConstructor(const Reflection &R) {
  return dyn_cast_or_null<CXXConstructorDecl>(getReachableDecl(R));
}

/// Returns the reflected conversion operator.
static const CXXConversionDecl *getReachableConversion(const Reflection &R) {
  return dyn_cast_or_null<CXXConversionDecl>(getReachableDecl(R));
}

/// Returns true if R designates a static member function.
static bool isStaticMemberFunction(const Reflection &R, APValue &Result) {
  if (const CXXMethodDecl *M = getAsMemberFunction(R))
    return SuccessBool(R, Result, M->isStatic());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a nonstatic member function.
static bool isNonstaticMemberFunction(const Reflection &R, APValue &Result) {
  if (const CXXMethodDecl *M = getAsMemberFunction(R))
    return SuccessBool(R, Result, M->isInstance());
  return SuccessFalse(R, Result);
}

/// Returns true if Args[1] designates a normal member function.
static bool isNormal(ReflectionQueryEvaluator &Eval,
                     SmallVectorImpl<APValue> &Args,
                     APValue &Result) {
  Reflection R(Eval.getContext(), Args[1]);
  if (const Decl *D = getReachableDecl(R)) {
    return SuccessBool(Eval, Result, D->getKind() == Decl::Kind::CXXMethod);
  }
  return SuccessFalse(Eval, Result);
}

/// Returns true if R designates an overridden member function.
static bool isOverride(const Reflection &R, APValue &Result) {
  if (const CXXMethodDecl *M = getAsMemberFunction(R))
    return SuccessBool(R, Result, M->size_overridden_methods() > 0);
  return SuccessFalse(R, Result);
}

/// returns true if R designates a member function with
/// the override specifier present.
static bool isOverrideSpecified(const Reflection &R, APValue &Result) {
  if (const CXXMethodDecl *M = getAsMemberFunction(R))
    return SuccessBool(R, Result, M->hasAttr<OverrideAttr>());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a deleted member function..
static bool isDeleted(const Reflection &R, APValue &Result) {
  if (const CXXMethodDecl *M = getAsMemberFunction(R))
    return SuccessBool(R, Result, M->isDeleted());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a virtual member function.
static bool isVirtual(const Reflection &R, APValue &Result) {
  if (const CXXMethodDecl *M = getAsMemberFunction(R))
    return SuccessBool(R, Result, M->isVirtual());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a pure virtual member function.
static bool isPureVirtual(const Reflection &R, APValue &Result) {
  if (const CXXMethodDecl *M = getAsMemberFunction(R))
    return SuccessBool(R, Result, M->isVirtual() && M->isPure());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a constructor.
static bool isConstructor(const Reflection &R,APValue &Result) {
  if (getReachableConstructor(R))
    return SuccessTrue(R, Result);
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a default constructor.
static bool isDefaultConstructor(const Reflection &R,APValue &Result) {
  if (const CXXConstructorDecl *CD = getReachableConstructor(R))
    return SuccessBool(R, Result, CD->isDefaultConstructor());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a copy constructor.
static bool isCopyConstructor(const Reflection &R,APValue &Result) {
  if (const CXXConstructorDecl *CD = getReachableConstructor(R))
    return SuccessBool(R, Result, CD->isCopyConstructor());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a copy constructor.
static bool isMoveConstructor(const Reflection &R,APValue &Result) {
  if (const CXXConstructorDecl *CD = getReachableConstructor(R))
    return SuccessBool(R, Result, CD->isMoveConstructor());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a copy assignment operator
static bool isCopyAssignmentOperator(const Reflection &R, APValue &Result) {
  if (const CXXMethodDecl *M = getAsMemberFunction(R))
    return SuccessBool(R, Result, M->isCopyAssignmentOperator());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a move assignment operator
static bool isMoveAssignmentOperator(const Reflection &R, APValue &Result) {
  if (const CXXMethodDecl *M = getAsMemberFunction(R))
    return SuccessBool(R, Result, M->isMoveAssignmentOperator());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates an enumerator.
static bool isDestructor(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R))
    return SuccessBool(R, Result, isa<CXXDestructorDecl>(D));
  return SuccessFalse(R, Result);
}

/// Returns true if Args[1] designates a conversion member function.
static bool isConversion(ReflectionQueryEvaluator &Eval,
                         SmallVectorImpl<APValue> &Args,
                         APValue &Result) {
  Reflection R(Eval.getContext(), Args[1]);
  return SuccessBool(Eval, Result, getReachableConversion(R));
}

/// Returns true if R designates a defaulted member function.
static bool isDefaulted(const Reflection &R, APValue &Result) {
  if (const CXXMethodDecl *M = getAsMemberFunction(R))
    return SuccessBool(R, Result, M->isDefaulted());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a explicit member function.
static bool isExplicit(const Reflection &R, APValue &Result) {
  if (const CXXConstructorDecl *CD = getReachableConstructor(R))
    return SuccessBool(R, Result, CD->isExplicit());
  if (const CXXConversionDecl *CD = getReachableConversion(R))
    return SuccessBool(R, Result, CD->isExplicit());
  return SuccessFalse(R, Result);
}

static AccessSpecifier getAccessUnmodified(const Reflection &R) {
  if (const CXXBaseSpecifier *B = getReachableBase(R))
    return B->getAccessSpecifier();

  if (const Decl *D = getReachableAliasDecl(R))
    return D->getAccess();

  return AS_none;
}

/// Returns true if R has specified access.
static bool hasAccess(const Reflection &R, APValue &Result) {
  AccessSpecifier AS = getAccessUnmodified(R);
  return SuccessBool(R, Result, AS != AS_none);
}

static AccessSpecifier getAccess(const Reflection &R) {
  switch (R.getModifiers().getAccessModifier()) {
  case AccessModifier::NotModified: {
    return getAccessUnmodified(R);
  }
  case AccessModifier::Default: {
    if (const Decl *D = getReachableAliasDecl(R)) {
      const TagDecl *TD = cast<TagDecl>(D->getDeclContext());
      return TD->getDefaultAccessSpecifier();
    }
    if (getReachableBase(R))
      return AS_private;
    return AS_none;
  }
  case AccessModifier::Public:
    return AS_public;
  case AccessModifier::Protected:
    return AS_protected;
  case AccessModifier::Private:
    return AS_private;
  }
  llvm_unreachable("unknown access modifier");
}

/// Returns true if R has public access.
static bool isPublic(const Reflection &R, APValue &Result) {
  return SuccessBool(R, Result, getAccess(R) == AS_public);
}

/// Returns true if R has protected access.
static bool isProtected(const Reflection &R, APValue &Result) {
  return SuccessBool(R, Result, getAccess(R) == AS_protected);
}

/// Returns true if R has private access.
static bool isPrivate(const Reflection &R, APValue &Result) {
  return SuccessBool(R, Result, getAccess(R) == AS_private);
}

/// Returns true if R has default access.
static bool hasDefaultAccess(const Reflection &R, APValue &Result) {
  if (const CXXBaseSpecifier *BS = getReachableBase(R))
    return SuccessBool(R, Result, BS->getAccessSpecifierAsWritten() == AS_none);

  if (const Decl *D = getReachableAliasDecl(R)) {
    if (const RecordDecl *RD = dyn_cast<RecordDecl>(D->getDeclContext())) {
      for (const Decl *CurDecl : dyn_cast<DeclContext>(RD)->decls()) {
        if (isa<AccessSpecDecl>(CurDecl))
          return SuccessFalse(R, Result);
        if (CurDecl == D)
          return SuccessTrue(R, Result);
      }
    }
  }

  return SuccessFalse(R, Result);
}

/// Linkage

/// Returns true if R has linkage.
static bool hasLinkage(ReflectionQueryEvaluator &Eval,
                       SmallVectorImpl<APValue> &Args,
                       APValue &Result) {
  Reflection R(Eval.getContext(), Args[1]);
  if (const Decl *D = getReachableDecl(R)) {
    if (const NamedDecl *ND = dyn_cast<NamedDecl>(D))
      return SuccessBool(Eval, Result, ND->hasLinkage());
  }
  return SuccessFalse(Eval, Result);
}

/// Returns true if R is externally linked.
static bool isExternallyLinked(ReflectionQueryEvaluator &Eval,
                               SmallVectorImpl<APValue> &Args,
                               APValue &Result) {
  Reflection R(Eval.getContext(), Args[1]);
  if (const Decl *D = getReachableDecl(R)) {
    if (const NamedDecl *ND = dyn_cast<NamedDecl>(D))
      return SuccessBool(Eval, Result, ND->hasExternalFormalLinkage());
  }
  return SuccessFalse(Eval, Result);
}

static bool isInternallyLinked(ReflectionQueryEvaluator &Eval,
                               SmallVectorImpl<APValue> &Args,
                               APValue &Result) {
  Reflection R(Eval.getContext(), Args[1]);
  if (const Decl *D = getReachableDecl(R)) {
    if (const NamedDecl *ND = dyn_cast<NamedDecl>(D))
      return SuccessBool(Eval, Result, ND->hasInternalFormalLinkage());
  }
  return SuccessFalse(Eval, Result);
}

/// Initializers

static bool hasInitializer(ReflectionQueryEvaluator &Eval,
                           SmallVectorImpl<APValue> &Args,
                           APValue &Result) {
  Reflection R(Eval.getContext(), Args[1]);
  if (const VarDecl *D = getAsVarDecl(R))
    return SuccessBool(Eval, Result, D->hasInit());
  if (const FieldDecl *D = getAsDataMember(R))
    return SuccessBool(Eval, Result, D->hasInClassInitializer());
  return SuccessFalse(Eval, Result);
}

/// General purpose

/// Returns true if Args[1] designates a decl declared with extern
/// specified.
template<typename T>
static bool isExternSpecified(ReflectionQueryEvaluator &Eval,
                              T *D, APValue &Result) {
  StorageClass SC = D->getStorageClass();
  return SuccessBool(Eval, Result, SC == SC_Extern);
}

static bool isExternSpecified(ReflectionQueryEvaluator &Eval,
                              SmallVectorImpl<APValue> &Args,
                              APValue &Result) {
  Reflection R(Eval.getContext(), Args[1]);
  if (const Decl *D = getReachableDecl(R)) {
    if (const FunctionDecl *FD = dyn_cast<FunctionDecl>(D))
      return isExternSpecified(Eval, FD, Result);

    if (const VarDecl *VD = dyn_cast<VarDecl>(D))
      return isExternSpecified(Eval, VD, Result);
  }
  return SuccessFalse(Eval, Result);
}

/// Returns true if Args[1] designates an inline decl.
static bool isInline(ReflectionQueryEvaluator &Eval,
                     SmallVectorImpl<APValue> &Args,
                     APValue &Result) {
  Reflection R(Eval.getContext(), Args[1]);
  if (const Decl *D = getReachableDecl(R)) {
    if (const VarDecl *VD = dyn_cast<VarDecl>(D))
      return SuccessBool(Eval, Result, VD->isInline());
    if (const FunctionDecl *FD = dyn_cast<FunctionDecl>(D))
      return SuccessBool(Eval, Result, FD->isInlined());
    if (const NamespaceDecl *NS = dyn_cast<NamespaceDecl>(D))
      return SuccessBool(Eval, Result, NS->isInline());
  }
  return SuccessFalse(Eval, Result);
}

/// Returns true if Args[1] designates a decl declared with inline
/// specified.
static bool isInlineSpecified(ReflectionQueryEvaluator &Eval,
                              SmallVectorImpl<APValue> &Args,
                              APValue &Result) {
  Reflection R(Eval.getContext(), Args[1]);
  if (const Decl *D = getReachableDecl(R)) {
    if (const VarDecl *VD = dyn_cast<VarDecl>(D))
      return SuccessBool(Eval, Result, VD->isInlineSpecified());
    if (const FunctionDecl *FD = dyn_cast<FunctionDecl>(D))
      return SuccessBool(Eval, Result, FD->isInlineSpecified());
  }
  return SuccessFalse(Eval, Result);
}

/// Returns true if Args[1] designate a constexpr decl.
static bool isConstexpr(ReflectionQueryEvaluator &Eval,
                        SmallVectorImpl<APValue> &Args,
                        APValue &Result) {
  Reflection R(Eval.getContext(), Args[1]);
  if (const Decl *D = getReachableDecl(R)) {
    if (const VarDecl *VD = dyn_cast<VarDecl>(D))
      return SuccessBool(Eval, Result, VD->isConstexpr());
    if (const FunctionDecl *FD = dyn_cast<FunctionDecl>(D))
      return SuccessBool(Eval, Result, FD->isConstexpr());
  }
  return SuccessFalse(Eval, Result);
}

/// Returns true if Args[1] designate a consteval decl.
static bool isConsteval(ReflectionQueryEvaluator &Eval,
                        SmallVectorImpl<APValue> &Args,
                        APValue &Result) {
  Reflection R(Eval.getContext(), Args[1]);
  if (const Decl *D = getReachableDecl(R)) {
    if (const FunctionDecl *FD = dyn_cast<FunctionDecl>(D))
      return SuccessBool(Eval, Result, FD->isConsteval());
  }
  return SuccessFalse(Eval, Result);
}

/// Returns true if Args[1] designate a final decl.
static bool isFinal(ReflectionQueryEvaluator &Eval,
                    SmallVectorImpl<APValue> &Args,
                    APValue &Result) {
  Reflection R(Eval.getContext(), Args[1]);
  if (const Decl *D = getReachableDecl(R)) {
    return SuccessBool(Eval, Result, D->hasAttr<FinalAttr>());
  }
  return SuccessFalse(Eval, Result);
}

/// Returns true if Args[1] designate a defined declaration.
static bool isDefined(ReflectionQueryEvaluator &Eval,
                      SmallVectorImpl<APValue> &Args,
                      APValue &Result) {
  Reflection R(Eval.getContext(), Args[1]);
  if (const Decl *D = getReachableDecl(R)) {
    if (const VarDecl *VD = dyn_cast<VarDecl>(D))
      return SuccessBool(Eval, Result, VD->isThisDeclarationADefinition());
    if (const FunctionDecl *FD = dyn_cast<FunctionDecl>(D))
      return SuccessBool(Eval, Result, FD->isThisDeclarationADefinition());
  }
  return SuccessFalse(Eval, Result);
}

/// Returns true if Args[1] designate a complete declaration.
static bool isComplete(ReflectionQueryEvaluator &Eval,
                       SmallVectorImpl<APValue> &Args,
                       APValue &Result) {
  Reflection R(Eval.getContext(), Args[1]);
  if (const Decl *D = getReachableDecl(R)) {
    if (const EnumDecl *ED = dyn_cast<EnumDecl>(D))
      return SuccessBool(Eval, Result, ED->isComplete());
    if (const TagDecl *TD = dyn_cast<TagDecl>(D))
      return SuccessBool(Eval, Result, TD->isThisDeclarationADefinition());
  }
  return SuccessFalse(Eval, Result);
}

/// Returns true if R designates a namespace.
static bool isNamespace(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R)) {
    bool IsNamespace = isa<NamespaceDecl>(D) ||
                       isa<NamespaceAliasDecl>(D) ||
                       isa<TranslationUnitDecl>(D);
    return SuccessBool(R, Result, IsNamespace);
  }
  return SuccessFalse(R, Result);
}

/// Aliases

/// Returns true if R designates a namespace alias.
static bool isNamespaceAlias(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableAliasDecl(R))
    return SuccessBool(R, Result, isa<NamespaceAliasDecl>(D));
  return SuccessFalse(R, Result);
}

/// Returns true if R designates an type alias.
static bool isTypeAlias(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableAliasDecl(R))
    return SuccessBool(R, Result, isa<TypedefNameDecl>(D));
  return SuccessFalse(R, Result);
}

/// Returns true if R designates an alias template.
static bool isAliasTemplate(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableAliasDecl(R))
    return SuccessBool(R, Result, isa<TypeAliasTemplateDecl>(D));
  return SuccessFalse(R, Result);
}

static const EnumDecl *getReachableEnumDecl(const Reflection &R) {
  if (const Decl *D = getReachableDecl(R))
    return dyn_cast<EnumDecl>(D);
  return nullptr;
}

/// Returns true if R designates an unscoped enum.
static bool isUnscopedEnum(const Reflection &R, APValue &Result) {
  if (const EnumDecl *D = getReachableEnumDecl(R))
    return SuccessBool(R, Result, !D->isScoped());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a scoped enum.
static bool isScopedEnum(const Reflection &R, APValue &Result) {
  if (const EnumDecl *D = getReachableEnumDecl(R))
    return SuccessBool(R, Result, D->isScoped());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates an enumerator.
static bool isEnumerator(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R))
    return SuccessBool(R, Result, isa<EnumConstantDecl>(D));
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a template.
static bool isTemplate(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R))
    return SuccessBool(R, Result, D->isTemplated());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a class template.
static bool isClassTemplate(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R)) {
    if (isa<ClassTemplateDecl>(D))
      return SuccessTrue(R, Result);

    if (const CXXRecordDecl *RD = dyn_cast<CXXRecordDecl>(D))
      return SuccessBool(R, Result, RD->isClass() && RD->isTemplated());
  }
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a function template.
static bool isFunctionTemplate(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R)) {
    if (isa<FunctionTemplateDecl>(D))
      return SuccessTrue(R, Result);

    if (const FunctionDecl *FD = dyn_cast<FunctionDecl>(D))
      return SuccessBool(R, Result, FD->isTemplated());
  }
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a variable template.
static bool isVariableTemplate(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R))
    return SuccessBool(R, Result, isa<VarTemplateDecl>(D));
  return SuccessFalse(R, Result);
}

/// Returns the reflected template member function.
static const CXXMethodDecl *getAsTemplateMemberFunction(const Reflection &R) {
  if (const Decl *D = getReachableDecl(R)) {
    if (const FunctionTemplateDecl *FTD = dyn_cast<FunctionTemplateDecl>(D))
      return dyn_cast<CXXMethodDecl>(FTD->getTemplatedDecl());

    if (const CXXMethodDecl *RD = dyn_cast<CXXMethodDecl>(D)) {
      if (RD->isTemplated())
        return RD;
    }
  }
  return nullptr;
}

/// Returns true if R designates a static member function template.
static bool isStaticMemberFunctionTemplate(const Reflection &R,
                                           APValue &Result) {
  if (const CXXMethodDecl *D = getAsTemplateMemberFunction(R))
    return SuccessBool(R, Result, D->isStatic());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a nonstatic member function template.
static bool isNonstaticMemberFunctionTemplate(const Reflection &R,
                                              APValue &Result) {
  if (const CXXMethodDecl *D = getAsTemplateMemberFunction(R))
    return SuccessBool(R, Result, D->isInstance());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a constructor template.
static bool isConstructorTemplate(const Reflection &R, APValue &Result) {
  if (const CXXMethodDecl *D = getAsTemplateMemberFunction(R))
    return SuccessBool(R, Result, isa<CXXConstructorDecl>(D));
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a destructor template.
static bool isDestructorTemplate(const Reflection &R, APValue &Result) {
  if (const CXXMethodDecl *D = getAsTemplateMemberFunction(R))
    return SuccessBool(R, Result, isa<CXXDestructorDecl>(D));
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a concept.
static bool isConcept(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R))
    return SuccessBool(R, Result, isa<ConceptDecl>(D));
  return SuccessFalse(R, Result);
}

static bool isPartialTemplateSpecialization(const Decl *D) {
  if (isa<ClassTemplatePartialSpecializationDecl>(D))
    return true;

  if (isa<VarTemplatePartialSpecializationDecl>(D))
    return true;

  return false;
}

static bool isTemplateSpecialization(const Decl *D) {
  if (isa<ClassTemplateSpecializationDecl>(D))
    return true;

  if (isa<ClassScopeFunctionSpecializationDecl>(D))
    return true;

  if (isa<VarTemplateSpecializationDecl>(D))
    return true;

  return isPartialTemplateSpecialization(D);
}

/// Returns true if R designates a specialized template.
static bool isSpecialization(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R))
    return SuccessBool(R, Result, isTemplateSpecialization(D));
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a partially specialized template.
static bool isPartialSpecialization(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R))
    return SuccessBool(R, Result, isPartialTemplateSpecialization(D));
  return SuccessFalse(R, Result);
}

// TODO: This currently uses TSK_Undeclared as a catch all
// for any issues, should this be a different state?
static TemplateSpecializationKind
getTemplateSpecializationKind(const Reflection &R) {
  const Decl *D = getReachableDecl(R);

  if (!D)
    return TSK_Undeclared;

  if (const CXXRecordDecl *RD = dyn_cast<CXXRecordDecl>(D))
    return RD->getTemplateSpecializationKind();

  if (const VarDecl *VD = dyn_cast<VarDecl>(D))
    return VD->getTemplateSpecializationKind();

  if (const FunctionDecl *FD = dyn_cast<FunctionDecl>(D))
    return FD->getTemplateSpecializationKind();

  if (const EnumDecl *ED = dyn_cast<EnumDecl>(D))
    return ED->getTemplateSpecializationKind();

  return TSK_Undeclared;
}

/// Returns true if R designates a explicitly specialized template.
static bool isExplicitSpecialization(const Reflection &R, APValue &Result) {
  if (TemplateSpecializationKind TSK = getTemplateSpecializationKind(R))
    return SuccessBool(R, Result, TSK == TSK_ExplicitSpecialization);
  return SuccessFalse(R, Result);
}

/// Returns true if R designates an implicitly instantiated template.
static bool isImplicitInstantiation(const Reflection &R, APValue &Result) {
  if (TemplateSpecializationKind TSK = getTemplateSpecializationKind(R))
    return SuccessBool(R, Result, TSK == TSK_ImplicitInstantiation);
  return SuccessFalse(R, Result);
}

/// Returns true if R designates an explicitly instantiated template.
static bool isExplicitInstantiation(const Reflection &R, APValue &Result) {
  if (TemplateSpecializationKind TSK = getTemplateSpecializationKind(R))
    return SuccessBool(R, Result, TSK == TSK_ExplicitInstantiationDeclaration
                               || TSK == TSK_ExplicitInstantiationDefinition);
  return SuccessFalse(R, Result);
}

/// Returns true if Args[1] designates a direct base.
static bool isDirectBase(ReflectionQueryEvaluator &Eval,
                         SmallVectorImpl<APValue> &Args,
                         APValue &Result) {
  Reflection R(Eval.getContext(), Args[1]);
  if (const CXXBaseSpecifier *B = getReachableBase(R))
    return SuccessBool(Eval, Result, !B->isVirtual());
  return SuccessFalse(Eval, Result);
}

/// Returns true if Args[1] designates a virtual base.
static bool isVirtualBase(ReflectionQueryEvaluator &Eval,
                          SmallVectorImpl<APValue> &Args,
                          APValue &Result) {
  Reflection R(Eval.getContext(), Args[1]);
  if (const CXXBaseSpecifier *B = getReachableBase(R))
    return SuccessBool(Eval, Result, B->isVirtual());
  return SuccessFalse(Eval, Result);

}

/// Returns true if R designates a function parameter.
static bool isFunctionParameter(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R))
    return SuccessBool(R, Result, isa<ParmVarDecl>(D));
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a type template parameter.
static bool isTypeTemplateParameter(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R))
    return SuccessBool(R, Result, D->getKind() == Decl::TemplateTypeParm);
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a nontype template parameter.
static bool isNontypeTemplateParameter(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R))
    return SuccessBool(R, Result, D->getKind() == Decl::NonTypeTemplateParm);
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a template template parameter.
static bool isTemplateTemplateParameter(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R))
    return SuccessBool(R, Result, D->getKind() == Decl::TemplateTemplateParm);
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a parameter with a default argument.
static bool hasDefaultArgument(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R)) {
    if (const auto *PVD = dyn_cast<ParmVarDecl>(D))
      return SuccessBool(R, Result, PVD->hasDefaultArg());
    if (const auto *TPVD = dyn_cast<NonTypeTemplateParmDecl>(D))
      return SuccessBool(R, Result, TPVD->hasDefaultArgument());
    if (const auto *TPVD = dyn_cast<TemplateTemplateParmDecl>(D))
      return SuccessBool(R, Result, TPVD->hasDefaultArgument());
    if (const auto *TPVD = dyn_cast<TemplateTypeParmDecl>(D))
      return SuccessBool(R, Result, TPVD->hasDefaultArgument());
  }
  return SuccessFalse(R, Result);
}

using OptionalAPValue = llvm::Optional<APValue>;
using OptionalAPSInt  = llvm::Optional<llvm::APSInt>;
using OptionalUInt    = llvm::Optional<std::uint64_t>;
using OptionalBool    = llvm::Optional<bool>;
using OptionalExpr    = llvm::Optional<const Expr *>;
using OptionalString  = llvm::Optional<std::string>;

static OptionalAPValue getArgAtIndex(const ArrayRef<APValue> &Args,
                                     std::size_t I) {
  if (I < Args.size())
    return Args[I];
  return { };
}

static OptionalAPSInt
getArgAsAPSInt(const ArrayRef<APValue> &Args, std::size_t I) {
  if (OptionalAPValue V = getArgAtIndex(Args, I))
    if (V->isInt())
      return V->getInt();
  return { };
}

static OptionalUInt
getArgAsUInt(const ArrayRef<APValue> &Args, std::size_t I) {
  if (OptionalAPSInt V = getArgAsAPSInt(Args, I))
    if (V->isUnsigned())
      return V->getZExtValue();
  return { };
}

static OptionalBool
getArgAsBool(const ArrayRef<APValue> &Args, std::size_t I) {
  if (OptionalAPSInt V = getArgAsAPSInt(Args, I))
    return V->getExtValue();
  return { };
}

static OptionalExpr
getArgAsExpr(const ArrayRef<APValue> &Args, std::size_t I) {
  if (OptionalAPValue V = getArgAtIndex(Args, I))
    if (V->isLValue())
      return V->getLValueBase().get<const Expr *>();
  return { };
}

static OptionalString
getArgAsString(const ArrayRef<APValue> &Args, std::size_t I) {
  if (OptionalExpr E = getArgAsExpr(Args, I))
    if (const StringLiteral *SL = dyn_cast<StringLiteral>(*E))
      return { SL->getString().str() };
  return { };
}

/// Returns true if Args[1] has an attribute with a name
/// equivalent to Args[2].
static bool hasAttribute(ReflectionQueryEvaluator &Eval,
                         SmallVectorImpl<APValue> &Args,
                         APValue &Result) {
  Reflection R(Eval.getContext(), Args[1]);

  OptionalString OptAttributeToFind = getArgAsString(Args, 2);
  if (!OptAttributeToFind)
    return SuccessFalse(Eval, Result);

  if (const Decl *D = getReachableDecl(R)) {
    for (Attr *A : D->attrs()) {
      if (A->getSpelling() == *OptAttributeToFind)
        return SuccessTrue(Eval, Result);
    }
  }

  return SuccessFalse(Eval, Result);
}

/// Returns true if R designates a type.
static bool isType(const Reflection &R, APValue &Result) {
  return SuccessBool(R, Result, R.isType());
}

/// Returns true if R designates a fundamental type.
static bool isFundamentalType(const Reflection &R, APValue &Result) {
  if (const MaybeType T = getCanonicalType(R))
    return SuccessBool(R, Result, T->isFundamentalType());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates an arithmetic type.
static bool isArithmeticType(const Reflection &R, APValue &Result) {
  if (const MaybeType T = getCanonicalType(R))
    return SuccessBool(R, Result, T->isArithmeticType());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a scalar type.
static bool isScalarType(const Reflection &R, APValue &Result) {
  if (const MaybeType T = getCanonicalType(R))
    return SuccessBool(R, Result, T->isScalarType());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates an object type.
static bool isObjectType(const Reflection &R, APValue &Result) {
  if (const MaybeType T = getCanonicalType(R))
    return SuccessBool(R, Result, T->isObjectType());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a compound type.
static bool isCompoundType(const Reflection &R, APValue &Result) {
  if (const MaybeType T = getCanonicalType(R))
    return SuccessBool(R, Result, T->isCompoundType());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a function type.
static bool isFunctionType(const Reflection &R, APValue &Result) {
  if (MaybeType T = getCanonicalType(R)) {
    return SuccessBool(R, Result, T->isFunctionType());
  }
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a class type.
static bool isClassType(const Reflection &R, APValue &Result) {
  if (MaybeType T = getCanonicalType(R)) {
    return SuccessBool(R, Result, T->isRecordType());
  }
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a union type.
static bool isUnionType(const Reflection &R, APValue &Result) {
  if (MaybeType T = getCanonicalType(R))
    return SuccessBool(R, Result, T->isUnionType());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates an unscoped enum type.
static bool isUnscopedEnumType(const Reflection &R, APValue &Result) {
  if (MaybeType T = getCanonicalType(R))
    return SuccessBool(R, Result, T->isEnumeralType());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a scoped enum type.
static bool isScopedEnumType(const Reflection &R, APValue &Result) {
  if (MaybeType T = getCanonicalType(R))
    return SuccessBool(R, Result, T->isScopedEnumeralType());
  return SuccessFalse(R, Result);
}

/// Returns true if R has void type.
static bool isVoidType(const Reflection &R, APValue &Result) {
  if (MaybeType T = getCanonicalType(R))
    return SuccessBool(R, Result, T->isVoidType());
  return SuccessFalse(R, Result);
}

/// Returns true if R has nullptr type.
static bool isNullPtrType(const Reflection &R, APValue &Result) {
  if (MaybeType T = getCanonicalType(R))
    return SuccessBool(R, Result, T->isNullPtrType());
  return SuccessFalse(R, Result);
}

/// Returns true if R has integral type.
static bool isIntegralType(const Reflection &R, APValue &Result) {
  if (MaybeType T = getCanonicalType(R))
    return SuccessBool(R, Result, T->isIntegralOrEnumerationType());
  return SuccessFalse(R, Result);
}

/// Returns true if R has floating point type.
static bool isFloatingPointType(const Reflection &R, APValue &Result) {
  if (MaybeType T = getCanonicalType(R))
    return SuccessBool(R, Result, T->isFloatingType());
  return SuccessFalse(R, Result);
}

/// Returns true if R has array type.
static bool isArrayType(const Reflection &R, APValue &Result) {
  if (MaybeType T = getCanonicalType(R))
    return SuccessBool(R, Result, T->isArrayType());
  return SuccessFalse(R, Result);
}

/// Returns true if R has pointer type.
static bool isPointerType(const Reflection &R, APValue &Result) {
  if (MaybeType T = getCanonicalType(R))
    return SuccessBool(R, Result, T->isPointerType());
  return SuccessFalse(R, Result);
}

/// Returns true if R has lvalue reference type.
static bool isLValueReferenceType(const Reflection &R, APValue &Result) {
  if (MaybeType T = getCanonicalType(R))
    return SuccessBool(R, Result, T->isLValueReferenceType());
  return SuccessFalse(R, Result);
}

/// Returns true if R has rvalue reference type.
static bool isRValueReferenceType(const Reflection &R, APValue &Result) {
  if (MaybeType T = getCanonicalType(R))
    return SuccessBool(R, Result, T->isRValueReferenceType());
  return SuccessFalse(R, Result);
}

/// Returns true if R has member object pointer type.
static bool isMemberObjectPointerType(const Reflection &R, APValue &Result) {
  if (MaybeType T = getCanonicalType(R))
    return SuccessBool(R, Result, T->isMemberDataPointerType());
  return SuccessFalse(R, Result);
}

/// Returns true if R has member function pointer type.
static bool isMemberFunctionPointerType(const Reflection &R, APValue &Result) {
  if (MaybeType T = getCanonicalType(R))
    return SuccessBool(R, Result, T->isMemberFunctionPointerType());
  return SuccessFalse(R, Result);
}

static MaybeType getAsCompleteType(const Reflection &R) {
  if (MaybeType T = getCanonicalType(R)) {
    if (!T->isIncompleteType())
      return T;
  }

  return { QualType() };
}

static const RecordType *getAsCompleteRecordType(const Reflection &R) {
  if (MaybeType T = getAsCompleteType(R)) {
    return dyn_cast<RecordType>(*T);
  }
  return nullptr;
}

/// Returns true if R designates a closure type.
static bool isClosureType(const Reflection &R, APValue &Result) {
  if (const RecordType *T = getAsCompleteRecordType(R)) {
    RecordDecl *RTD = T->getDecl();
    return SuccessBool(R, Result, RTD->isLambda());
  }
  return SuccessFalse(R, Result);
}

/// Returns true if R designates an incomplete type.
static bool isIncompleteType(const Reflection &R, APValue &Result) {
  if (MaybeType T = getCanonicalType(R)) {
    return SuccessBool(R, Result, T->isIncompleteType());
  }
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a type with const specified.
static bool isConstType(const Reflection &R, APValue &Result) {
  if (MaybeType T = getCanonicalType(R)) {
    return SuccessBool(R, Result, (*T).isConstQualified());
  }
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a type with volatile specified.
static bool isVolatileType(const Reflection &R, APValue &Result) {
  if (MaybeType T = getCanonicalType(R)) {
    return SuccessBool(R, Result, (*T).isVolatileQualified());
  }
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a trivial type.
static bool isTrivialType(const Reflection &R, APValue &Result) {
  if (MaybeType T = getCanonicalType(R)) {
    ASTContext &Context = R.getContext();
    return SuccessBool(R, Result, (*T).isTrivialType(Context));
  }
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a trivially copyable type.
static bool isTriviallyCopyableType(const Reflection &R, APValue &Result) {
  if (MaybeType T = getCanonicalType(R)) {
    ASTContext &Context = R.getContext();
    return SuccessBool(R, Result, (*T).isTriviallyCopyableType(Context));
  }
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a standard layout type.
static bool isStandardLayoutType(const Reflection &R, APValue &Result) {
  if (MaybeType T = getCanonicalType(R)) {
    return SuccessBool(R, Result, T->isStandardLayoutType());
  }
  return SuccessFalse(R, Result);
}

/// Returns true if R deisgnates a POD.
static bool isPODType(const Reflection &R, APValue &Result) {
  if (MaybeType T = getCanonicalType(R)) {
    ASTContext &Context = R.getContext();
    return SuccessBool(R, Result, (*T).isPODType(Context));
  }
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a literal type.
static bool isLiteralType(const Reflection &R, APValue &Result) {
  if (MaybeType T = getCanonicalType(R)) {
    ASTContext &Context = R.getContext();
    return SuccessBool(R, Result, T->isLiteralType(Context));
  }
  return SuccessFalse(R, Result);
}

/// Returns true if R designates an empty type.
static bool isEmptyType(const Reflection &R, APValue &Result) {
  if (const RecordType *T = getAsCompleteRecordType(R)) {
    CXXRecordDecl *RTD = cast<CXXRecordDecl>(T->getDecl());
    return SuccessBool(R, Result, RTD->isEmpty());
  }
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a polymorphic type.
static bool isPolymorphicType(const Reflection &R, APValue &Result) {
  if (const RecordType *T = getAsCompleteRecordType(R)) {
    CXXRecordDecl *RTD = cast<CXXRecordDecl>(T->getDecl());
    return SuccessBool(R, Result, RTD->isPolymorphic());
  }
  return SuccessFalse(R, Result);
}

/// Returns true if R designates an abstract type.
static bool isAbstractType(const Reflection &R, APValue &Result) {
  if (const RecordType *T = getAsCompleteRecordType(R)) {
    CXXRecordDecl *RTD = cast<CXXRecordDecl>(T->getDecl());
    return SuccessBool(R, Result, RTD->isAbstract());
  }
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a final type.
static bool isFinalType(const Reflection &R, APValue &Result) {
  if (const RecordType *T = getAsCompleteRecordType(R)) {
    CXXRecordDecl *RTD = cast<CXXRecordDecl>(T->getDecl());
    return SuccessBool(R, Result, RTD->hasAttr<FinalAttr>());
  }
  return SuccessFalse(R, Result);
}

/// Returns true if R designates an aggregate type.
static bool isAggregateType(const Reflection &R, APValue &Result) {
  if (MaybeType T = getAsCompleteType(R)) {
    return SuccessBool(R, Result, T->isAggregateType());
  }
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a signed type.
static bool isSignedType(const Reflection &R, APValue &Result) {
  if (MaybeType T = getCanonicalType(R)) {
    return SuccessBool(R, Result, T->isSignedIntegerType() || T->isFloatingType());
  }
  return SuccessFalse(R, Result);
}

/// Returns true if R designates an unsigned type.
static bool isUnsignedType(const Reflection &R, APValue &Result) {
  if (MaybeType T = getCanonicalType(R)) {
    return SuccessBool(R, Result, T->isUnsignedIntegerType());
  }
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a type with unique object representations.
static bool hasUniqueObjectRepresentationsType(const Reflection &R, APValue &Result) {
  if (MaybeType T = getCanonicalType(R)) {
    ASTContext &Context = R.getContext();
    return SuccessBool(R, Result, Context.hasUniqueObjectRepresentations(*T));
  }
  return SuccessFalse(R, Result);
}

/// Type operations

/// Provides a generic function for delegation, to specific
/// type trait evaluations via specializations.
///
/// Returns the result of evaluating the type trait
/// TypeTrait given the arguments [Args[1], Args[n]].
template<TypeTrait TypeTrait>
static bool delegateToTypeTrait(ReflectionQueryEvaluator &Eval,
                                SmallVectorImpl<APValue> &Args,
                                APValue &Result) {
  ASTContext &Context = Eval.getContext();

  // Convert the arguments to type source information.
  llvm::SmallVector<TypeSourceInfo *, 2> TypeTraitArgs;
  for (unsigned I = 1; I < Args.size(); ++I) {
    Reflection R(Context, Args[I]);

    MaybeType T = getCanonicalType(R);
    if (!T)
      return SuccessFalse(R, Result);

    TypeTraitArgs.push_back(Context.getTrivialTypeSourceInfo(T));
  }

  // Execute the query and return the results.
  ReflectionCallback *CB = Eval.getCallbacks();
  bool EvalResult = CB->EvalTypeTrait(TypeTrait, TypeTraitArgs);
  return SuccessBool(Eval, Result, EvalResult);
}

/// Captures

static bool hasDefaultRefCapture(const Reflection &R, APValue &Result) {
  return ErrorUnimplemented(R);
}

static bool hasDefaultCopyCapture(const Reflection &R, APValue &Result) {
  return ErrorUnimplemented(R);
}

static bool isCapture(const Reflection &R, APValue &Result) {
  return ErrorUnimplemented(R);
}

static bool isSimpleCapture(const Reflection &R, APValue &Result) {
  return ErrorUnimplemented(R);
}

static bool isRefCapture(const Reflection &R, APValue &Result) {
  return ErrorUnimplemented(R);
}

static bool isCopyCapture(const Reflection &R, APValue &Result) {
  return ErrorUnimplemented(R);
}

static bool isExplicitCapture(const Reflection &R, APValue &Result) {
  return ErrorUnimplemented(R);
}

static bool isInitCapture(const Reflection &R, APValue &Result) {
  return ErrorUnimplemented(R);
}

static bool hasCaptures(const Reflection &R, APValue &Result) {
  return ErrorUnimplemented(R);
}

/// Returns true if R designates an expression.
static bool isExpression(const Reflection &R, APValue &Result) {
  return SuccessBool(R, Result, R.isExpression());
}

/// Returns true if R designates an LValue expression.
static bool isLValue(const Reflection &R, APValue &Result) {
  if (const Expr *E = getExpr(R))
    return SuccessBool(R, Result, E->isLValue());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates an XValue expression.
static bool isXValue(const Reflection &R, APValue &Result) {
  if (const Expr *E = getExpr(R))
    return SuccessBool(R, Result, E->isXValue());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates an RValue expression.
static bool isPRValue(const Reflection &R, APValue &Result) {
  if (const Expr *E = getExpr(R))
    return SuccessBool(R, Result, E->isRValue());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a value.
static bool isValue(const Reflection &R, APValue &Result) {
  if (const Expr *E = getExpr(R)) {
    if (isa<IntegerLiteral>(E))
      return SuccessTrue(R, Result);
    if (isa<FixedPointLiteral>(E))
      return SuccessTrue(R, Result);
    if (isa<FloatingLiteral>(E))
      return SuccessTrue(R, Result);
    if (isa<CharacterLiteral>(E))
      return SuccessTrue(R, Result);
    if (isa<ImaginaryLiteral>(E))
      return SuccessTrue(R, Result);
    if (isa<StringLiteral>(E))
      return SuccessTrue(R, Result);
    if (isa<CompoundLiteralExpr>(E))
      return SuccessTrue(R, Result);
    if (isa<UserDefinedLiteral>(E))
      return SuccessTrue(R, Result);
    if (isa<CXXBoolLiteralExpr>(E))
      return SuccessTrue(R, Result);
    if (isa<CXXNullPtrLiteralExpr>(E))
      return SuccessTrue(R, Result);
  }
  return SuccessFalse(R, Result);
}

static bool isTypeOperation(ReflectionQuery Q) {
  return query_first_type_operation <= Q && Q <= query_last_type_operation;
}

static bool requiresCallbacks(ReflectionQuery Q) {
  return isTypeOperation(Q);
}

static Reflection makeReflection(ReflectionQueryEvaluator &Evaluator,
                                 APValue &Refl) {
  return Reflection(Evaluator.getContext(), Refl,
                    Evaluator.getQueryExpr(), Evaluator.getDiag());
}

bool ReflectionQueryEvaluator::EvaluatePredicate(SmallVectorImpl<APValue> &Args,
                                                 APValue &Result) {
  assert(isPredicateQuery(Query) && "invalid query");
  assert((!requiresCallbacks(Query) || CB) &&
         "query can only be used in a context where callbacks are provided");

  switch (Query) {
  case query_is_invalid:
    return ::isInvalid(makeReflection(*this, Args[1]), Result);
  case query_is_entity:
    return isEntity(makeReflection(*this, Args[1]), Result);
  case query_is_named:
    return isNamed(makeReflection(*this, Args[1]), Result);

  // Scopes

  case query_is_local:
    return isLocal(makeReflection(*this, Args[1]), Result);

  // Variables
  case query_is_variable:
    return isVariable(makeReflection(*this, Args[1]), Result);
  case query_has_static_storage:
    return hasStaticStorage(makeReflection(*this, Args[1]), Result);
  case query_has_thread_local_storage:
    return hasThreadLocalStorage(makeReflection(*this, Args[1]), Result);
  case query_has_automatic_local_storage:
    return hasAutomaticLocalStorage(makeReflection(*this, Args[1]), Result);

  // Functions
  case query_is_function:
    return isFunction(makeReflection(*this, Args[1]), Result);
  case query_is_nothrow:
    return isNothrow(makeReflection(*this, Args[1]), Result);

  // Classes
  case query_is_class:
    return isClass(makeReflection(*this, Args[1]), Result);
  case query_is_union:
    return isUnion(makeReflection(*this, Args[1]), Result);
  case query_has_virtual_destructor:
    return hasVirtualDestructor(makeReflection(*this, Args[1]), Result);
  case query_is_declared_struct:
    return isDeclaredStruct(*this, Args, Result);
  case query_is_declared_class:
    return isDeclaredClass(*this, Args, Result);

  // Class members
  case query_is_class_member:
    return isClassMember(makeReflection(*this, Args[1]), Result);

  // Data members
  case query_is_static_data_member:
    return isStaticDataMember(makeReflection(*this, Args[1]), Result);
  case query_is_nonstatic_data_member:
    return isNonstaticDataMember(makeReflection(*this, Args[1]), Result);
  case query_is_bit_field:
    return isBitField(makeReflection(*this, Args[1]), Result);
  case query_is_mutable:
    return isMutable(*this, Args, Result);

  // Member functions
  case query_is_static_member_function:
    return isStaticMemberFunction(makeReflection(*this, Args[1]), Result);
  case query_is_nonstatic_member_function:
    return isNonstaticMemberFunction(makeReflection(*this, Args[1]), Result);
  case query_is_normal:
    return isNormal(*this, Args, Result);
  case query_is_override:
    return isOverride(makeReflection(*this, Args[1]), Result);
  case query_is_override_specified:
    return isOverrideSpecified(makeReflection(*this, Args[1]), Result);
  case query_is_deleted:
    return isDeleted(makeReflection(*this, Args[1]), Result);
  case query_is_virtual:
    return isVirtual(makeReflection(*this, Args[1]), Result);
  case query_is_pure_virtual:
    return isPureVirtual(makeReflection(*this, Args[1]), Result);

  // Special members
  case query_is_constructor:
    return isConstructor(makeReflection(*this, Args[1]), Result);
  case query_is_default_constructor:
    return isDefaultConstructor(makeReflection(*this, Args[1]), Result);
  case query_is_copy_constructor:
    return isCopyConstructor(makeReflection(*this, Args[1]), Result);
  case query_is_move_constructor:
    return isMoveConstructor(makeReflection(*this, Args[1]), Result);
  case query_is_copy_assignment_operator:
    return isCopyAssignmentOperator(makeReflection(*this, Args[1]), Result);
  case query_is_move_assignment_operator:
    return isMoveAssignmentOperator(makeReflection(*this, Args[1]), Result);
  case query_is_destructor:
    return isDestructor(makeReflection(*this, Args[1]), Result);
  case query_is_conversion:
    return isConversion(*this, Args, Result);
   case query_is_defaulted:
    return isDefaulted(makeReflection(*this, Args[1]), Result);
  case query_is_explicit:
    return isExplicit(makeReflection(*this, Args[1]), Result);

  // Access
  case query_has_access:
    return hasAccess(makeReflection(*this, Args[1]), Result);
  case query_is_public:
    return isPublic(makeReflection(*this, Args[1]), Result);
  case query_is_protected:
    return isProtected(makeReflection(*this, Args[1]), Result);
  case query_is_private:
    return isPrivate(makeReflection(*this, Args[1]), Result);
  case query_has_default_access:
    return hasDefaultAccess(makeReflection(*this, Args[1]), Result);

  // Linkage
  case query_has_linkage:
    return hasLinkage(*this, Args, Result);
  case query_is_externally_linked:
    return isExternallyLinked(*this, Args, Result);
  case query_is_internally_linked:
    return isInternallyLinked(*this, Args, Result);

  // Initializers
  case query_has_initializer:
    return hasInitializer(*this, Args, Result);

  // General purpose
  case query_is_extern_specified:
    return isExternSpecified(*this, Args, Result);
  case query_is_inline:
    return isInline(*this, Args, Result);
  case query_is_inline_specified:
    return isInlineSpecified(*this, Args, Result);
  case query_is_constexpr:
    return isConstexpr(*this, Args, Result);
  case query_is_consteval:
    return isConsteval(*this, Args, Result);
  case query_is_final:
    return isFinal(*this, Args, Result);
  case query_is_defined:
    return isDefined(*this, Args, Result);
  case query_is_complete:
    return isComplete(*this, Args, Result);

  // Namespaces
  case query_is_namespace:
    return isNamespace(makeReflection(*this, Args[1]), Result);

  // Aliases
  case query_is_namespace_alias:
    return isNamespaceAlias(makeReflection(*this, Args[1]), Result);
  case query_is_type_alias:
    return isTypeAlias(makeReflection(*this, Args[1]), Result);
  case query_is_alias_template:
    return isAliasTemplate(makeReflection(*this, Args[1]), Result);

  // Enums
  case query_is_unscoped_enum:
    return isUnscopedEnum(makeReflection(*this, Args[1]), Result);
  case query_is_scoped_enum:
    return isScopedEnum(makeReflection(*this, Args[1]), Result);

  // Enumerators
  case query_is_enumerator:
    return isEnumerator(makeReflection(*this, Args[1]), Result);

  // Templates
  case query_is_template:
    return isTemplate(makeReflection(*this, Args[1]), Result);
  case query_is_class_template:
    return isClassTemplate(makeReflection(*this, Args[1]), Result);
  case query_is_function_template:
    return isFunctionTemplate(makeReflection(*this, Args[1]), Result);
  case query_is_variable_template:
    return isVariableTemplate(makeReflection(*this, Args[1]), Result);
  case query_is_static_member_function_template:
    return isStaticMemberFunctionTemplate(makeReflection(*this, Args[1]), Result);
  case query_is_nonstatic_member_function_template:
    return isNonstaticMemberFunctionTemplate(makeReflection(*this, Args[1]), Result);
  case query_is_constructor_template:
    return isConstructorTemplate(makeReflection(*this, Args[1]), Result);
  case query_is_destructor_template:
    return isDestructorTemplate(makeReflection(*this, Args[1]), Result);
  case query_is_concept:
    return isConcept(makeReflection(*this, Args[1]), Result);

  // Specializations
  case query_is_specialization:
    return isSpecialization(makeReflection(*this, Args[1]), Result);
  case query_is_partial_specialization:
    return isPartialSpecialization(makeReflection(*this, Args[1]), Result);
  case query_is_explicit_specialization:
    return isExplicitSpecialization(makeReflection(*this, Args[1]), Result);
  case query_is_implicit_instantiation:
    return isImplicitInstantiation(makeReflection(*this, Args[1]), Result);
  case query_is_explicit_instantiation:
    return isExplicitInstantiation(makeReflection(*this, Args[1]), Result);

  // Base class specifiers
  case query_is_direct_base:
    return isDirectBase(*this, Args, Result);
  case query_is_virtual_base:
    return isVirtualBase(*this, Args, Result);

  // Parameters
  case query_is_function_parameter:
    return isFunctionParameter(makeReflection(*this, Args[1]), Result);
  case query_is_type_template_parameter:
    return isTypeTemplateParameter(makeReflection(*this, Args[1]), Result);
  case query_is_nontype_template_parameter:
    return isNontypeTemplateParameter(makeReflection(*this, Args[1]), Result);
  case query_is_template_template_parameter:
    return isTemplateTemplateParameter(makeReflection(*this, Args[1]), Result);
  case query_has_default_argument:
    return hasDefaultArgument(makeReflection(*this, Args[1]), Result);

  // Attributes
  case query_has_attribute:
    return hasAttribute(*this, Args, Result);

  // Types
  case query_is_type:
    return ::isType(makeReflection(*this, Args[1]), Result);
  case query_is_fundamental_type:
    return isFundamentalType(makeReflection(*this, Args[1]), Result);
  case query_is_arithmetic_type:
    return isArithmeticType(makeReflection(*this, Args[1]), Result);
  case query_is_scalar_type:
    return isScalarType(makeReflection(*this, Args[1]), Result);
  case query_is_object_type:
    return isObjectType(makeReflection(*this, Args[1]), Result);
  case query_is_compound_type:
    return isCompoundType(makeReflection(*this, Args[1]), Result);
  case query_is_function_type:
    return isFunctionType(makeReflection(*this, Args[1]), Result);
  case query_is_class_type:
    return isClassType(makeReflection(*this, Args[1]), Result);
  case query_is_union_type:
    return isUnionType(makeReflection(*this, Args[1]), Result);
  case query_is_unscoped_enum_type:
    return isUnscopedEnumType(makeReflection(*this, Args[1]), Result);
  case query_is_scoped_enum_type:
    return isScopedEnumType(makeReflection(*this, Args[1]), Result);
  case query_is_void_type:
    return isVoidType(makeReflection(*this, Args[1]), Result);
  case query_is_null_pointer_type:
    return isNullPtrType(makeReflection(*this, Args[1]), Result);
  case query_is_integral_type:
    return isIntegralType(makeReflection(*this, Args[1]), Result);
  case query_is_floating_point_type:
    return isFloatingPointType(makeReflection(*this, Args[1]), Result);
  case query_is_array_type:
    return isArrayType(makeReflection(*this, Args[1]), Result);
  case query_is_pointer_type:
    return isPointerType(makeReflection(*this, Args[1]), Result);
  case query_is_lvalue_reference_type:
    return isLValueReferenceType(makeReflection(*this, Args[1]), Result);
  case query_is_rvalue_reference_type:
    return isRValueReferenceType(makeReflection(*this, Args[1]), Result);
  case query_is_member_object_pointer_type:
    return isMemberObjectPointerType(makeReflection(*this, Args[1]), Result);
  case query_is_member_function_pointer_type:
    return isMemberFunctionPointerType(makeReflection(*this, Args[1]), Result);
  case query_is_closure_type:
    return isClosureType(makeReflection(*this, Args[1]), Result);

  // Type properties
  case query_is_incomplete_type:
    return isIncompleteType(makeReflection(*this, Args[1]), Result);
  case query_is_const_type:
    return isConstType(makeReflection(*this, Args[1]), Result);
  case query_is_volatile_type:
    return isVolatileType(makeReflection(*this, Args[1]), Result);
  case query_is_trivial_type:
    return isTrivialType(makeReflection(*this, Args[1]), Result);
  case query_is_trivially_copyable_type:
    return isTriviallyCopyableType(makeReflection(*this, Args[1]), Result);
  case query_is_standard_layout_type:
    return isStandardLayoutType(makeReflection(*this, Args[1]), Result);
  case query_is_pod_type:
    return isPODType(makeReflection(*this, Args[1]), Result);
  case query_is_literal_type:
    return isLiteralType(makeReflection(*this, Args[1]), Result);
  case query_is_empty_type:
    return isEmptyType(makeReflection(*this, Args[1]), Result);
  case query_is_polymorphic_type:
    return isPolymorphicType(makeReflection(*this, Args[1]), Result);
  case query_is_abstract_type:
    return isAbstractType(makeReflection(*this, Args[1]), Result);
  case query_is_final_type:
    return isFinalType(makeReflection(*this, Args[1]), Result);
  case query_is_aggregate_type:
    return isAggregateType(makeReflection(*this, Args[1]), Result);
  case query_is_signed_type:
    return isSignedType(makeReflection(*this, Args[1]), Result);
  case query_is_unsigned_type:
    return isUnsignedType(makeReflection(*this, Args[1]), Result);
  case query_has_unique_object_representations_type:
    return hasUniqueObjectRepresentationsType(makeReflection(*this, Args[1]), Result);

  // Type operations
  case query_is_constructible:
    return delegateToTypeTrait<TT_IsConstructible>(*this, Args, Result);
  case query_is_trivially_constructible:
    return delegateToTypeTrait<TT_IsTriviallyConstructible>(*this, Args, Result);
  case query_is_nothrow_constructible:
    return delegateToTypeTrait<TT_IsNothrowConstructible>(*this, Args, Result);
  case query_is_assignable:
    return delegateToTypeTrait<BTT_IsAssignable>(*this, Args, Result);
  case query_is_trivially_assignable:
    return delegateToTypeTrait<BTT_IsTriviallyAssignable>(*this, Args, Result);
  case query_is_nothrow_assignable:
    return delegateToTypeTrait<BTT_IsNothrowAssignable>(*this, Args, Result);
  case query_is_destructible:
    return delegateToTypeTrait<UTT_IsDestructible>(*this, Args, Result);
  case query_is_trivially_destructible:
    return delegateToTypeTrait<UTT_IsTriviallyDestructible>(*this, Args, Result);
  case query_is_nothrow_destructible:
    return delegateToTypeTrait<UTT_IsNothrowDestructible>(*this, Args, Result);

  // Captures
  case query_has_default_ref_capture:
    return hasDefaultRefCapture(makeReflection(*this, Args[1]), Result);
  case query_has_default_copy_capture:
    return hasDefaultCopyCapture(makeReflection(*this, Args[1]), Result);
  case query_is_capture:
    return isCapture(makeReflection(*this, Args[1]), Result);
  case query_is_simple_capture:
    return isSimpleCapture(makeReflection(*this, Args[1]), Result);
  case query_is_ref_capture:
    return isRefCapture(makeReflection(*this, Args[1]), Result);
  case query_is_copy_capture:
    return isCopyCapture(makeReflection(*this, Args[1]), Result);
  case query_is_explicit_capture:
    return isExplicitCapture(makeReflection(*this, Args[1]), Result);
  case query_is_init_capture:
    return isInitCapture(makeReflection(*this, Args[1]), Result);
  case query_has_captures:
    return hasCaptures(makeReflection(*this, Args[1]), Result);

  // Expressions
  case query_is_expression:
    return ::isExpression(makeReflection(*this, Args[1]), Result);
  case query_is_lvalue:
    return isLValue(makeReflection(*this, Args[1]), Result);
  case query_is_xvalue:
    return isXValue(makeReflection(*this, Args[1]), Result);
  case query_is_prvalue:
    return isPRValue(makeReflection(*this, Args[1]), Result);
  case query_is_value:
    return isValue(makeReflection(*this, Args[1]), Result);

  default:
    break;
  }
  llvm_unreachable("invalid predicate selector");
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

/// Set Result to a reflection of T, at the given offset,
/// for a parent reflection P.
static bool makeReflection(QualType T, unsigned Offset, const APValue &P,
                           APValue &Result) {
  if (T.isNull())
    return makeReflection(Result);
  Result = APValue(RK_type, T.getAsOpaquePtr(), Offset, P);
  return true;
}

/// Set Result to a reflection of B, at the given offset,
/// for a parent reflection P.
static bool makeReflection(const CXXBaseSpecifier *B, unsigned Offset,
                           const APValue &P, APValue &Result) {
  if (B) {
    Result = APValue(RK_base_specifier, B, Offset, P);
    return true;
  }

  return makeReflection(Result);
}

/// Set Result to a copy of R with modifiers M applied.
static bool makeReflection(const Reflection &R,
                           const ReflectionModifiers &M, APValue &Result) {
  Result = APValue(R.getKind(), R.getOpaqueValue(), M);
  return true;
}

static bool getType(const Reflection &R, APValue &Result) {
  if (R.isType())
    return makeReflection(R.getAsType(), Result);

  if (const Expr *E = getExpr(R))
    return makeReflection(E->getType(), Result);

  if (const Decl *D = getReachableAliasDecl(R)) {
    if (const ValueDecl *VD = dyn_cast<ValueDecl>(D))
      return makeReflection(VD->getType(), Result);

    if (const TypeDecl *TD = dyn_cast<TypeDecl>(D)) {
      ASTContext &Context = R.getContext();
      return makeReflection(Context.getTypeDeclType(TD), Result);
    }
  }

  // FIXME: Emit an appropriate error diagnostic.
  return Error(R);
}

static bool getReturnType(const Reflection &R, APValue &Result) {
  if (MaybeType T = getCanonicalType(R)) {
    if (const FunctionType *FT = dyn_cast<FunctionType>(T.getTypePtr()))
      return makeReflection(FT->getReturnType(), Result);
  }
  if (const Decl *D = getReachableDecl(R)) {
    if (const FunctionDecl *FD = dyn_cast<FunctionDecl>(D))
      return makeReflection(FD->getReturnType(), Result);
  }
  return Error(R);
}

static bool getThisRefType(const Reflection &R, APValue &Result) {
  if (const CXXMethodDecl *M = getAsMemberFunction(R))
    return makeReflection(M->getThisType(), Result);
  return Error(R);
}

static bool getUnderlyingType(const Reflection &R, APValue &Result) {
  if (const EnumDecl *ED = getReachableEnumDecl(R))
    return makeReflection(ED->getIntegerType(), Result);
  return Error(R);
}

static bool getEntity(const Reflection &R, APValue &Result) {
  if (R.isType()) {
    /// The entity is the canonical type.
    QualType T = R.getContext().getCanonicalType(R.getAsType());
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
    QualType T = R.getContext().getCanonicalType(Base->getType());
    return makeReflection(T, Result);
  }
  return Error(R);
}

static bool getParent(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R))
    return makeReflection(D->getDeclContext(), Result);
  return Error(R);
}

static bool getDefinition(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R)) {
    return makeReflection(D, Result);
  }
  return Error(R);
}

/// True if D is reflectable. Some declarations are not reflected (e.g.,
/// access specifiers, non-canonical decls).
static bool isReflectableDecl(const Decl *D) {
  assert(D && "null declaration");
  if (isa<AccessSpecDecl>(D))
    return false;
  if (isa<CXXInjectorDecl>(D) && !D->getDeclContext()->isDependentContext())
    return false;
  if (const CXXRecordDecl *Class = dyn_cast<CXXRecordDecl>(D))
    if (Class->isInjectedClassName())
      return false;
  return D->getCanonicalDecl() == D;
}

/// Filter non-reflectable members.
static const Decl *findIterableMember(const Decl *D) {
  while (D && !isReflectableDecl(D))
    D = D->getNextDeclInContext();
  return D;
}

static const ParmVarDecl *getFirstFunctionParameter(const FunctionDecl *FD) {
  for (const ParmVarDecl *PVD : FD->parameters())
    return PVD;
  return nullptr;
}

static const QualType
getFunctionParameter(const FunctionType *FT, unsigned Index) {
  if (const FunctionProtoType *FPT = dyn_cast<FunctionProtoType>(FT)) {
    if (Index < FPT->getNumParams())
      return *(FPT->param_type_begin() + Index);
  }

  return QualType();
}

static bool
getFunctionParameter(const Reflection &R, unsigned Index, APValue &Result) {
  if (MaybeType T = getCanonicalType(R)) {
    if (const FunctionType *FT = dyn_cast<FunctionType>(T.getTypePtr())) {
      APValue ParentRefl;
      if (!makeReflection(T, ParentRefl))
        llvm_unreachable("function type reflection creation failed");

      QualType Param = getFunctionParameter(FT, Index);
      return makeReflection(Param, Index + 1, ParentRefl, Result);
    }
  }

  return Error(R);
}

/// Returns the first reflectable member.
static const Decl *legacyGetFirstMember(const DeclContext *DC) {
  if (const FunctionDecl *FD = dyn_cast<FunctionDecl>(DC))
    return getFirstFunctionParameter(FD);
  return findIterableMember(*DC->decls_begin());
}

static const ParmVarDecl *getNextFunctionParameter(const ParmVarDecl *D) {
  assert(isa<FunctionDecl>(D->getDeclContext()));
  const FunctionDecl *FD = cast<FunctionDecl>(D->getDeclContext());

  unsigned NextParamIndex = D->getFunctionScopeIndex() + 1;
  if (NextParamIndex < FD->getNumParams())
    return FD->getParamDecl(NextParamIndex);

  return nullptr;
}

/// Returns the next reflectable member
static const Decl *legacyGetNextMember(const Decl *D) {
  if (const ParmVarDecl *PVD = dyn_cast<ParmVarDecl>(D))
    return getNextFunctionParameter(PVD);
  return findIterableMember(D->getNextDeclInContext());
}

/// Returns the reachable declaration context for R, if any.
static const DeclContext *getReachableDeclContext(const Reflection &R) {
  if (const Decl *D = getReachableDecl(R)) {
    if (const DeclContext *DC = dyn_cast<DeclContext>(D))
      return DC;

    if (const TemplateDecl *TD = dyn_cast<TemplateDecl>(D))
      return dyn_cast<DeclContext>(TD->getTemplatedDecl());
  }

  return nullptr;
}

/// Returns the first member
static bool getBegin(const Reflection &R, APValue &Result) {
  if (const DeclContext *DC = getReachableDeclContext(R))
    return makeReflection(legacyGetFirstMember(DC), Result);
  return Error(R);
}

static bool getNext(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableAliasDecl(R))
    return makeReflection(legacyGetNextMember(D), Result);
  return Error(R);
}

/// Returns the first template parameter
static bool getBeginTemplateParam(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R)) {
    if (isa<TemplateDecl>(D)) {
      const TemplateParameterList* TPL =
        cast<TemplateDecl>(D)->getTemplateParameters();
      if (TPL->size()) {
        return makeReflection(TPL->getParam(0), Result);
      }
    }
  }

  return Error(R);
}

// Returns the next template parameter
static bool getNextTemplateParam(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R)) {
    unsigned Index = 0;
    if (isa<NonTypeTemplateParmDecl>(D)) {
      Index = cast<NonTypeTemplateParmDecl>(D)->getIndex();
    } else if (isa<TemplateTemplateParmDecl>(D)) {
      Index = cast<TemplateTemplateParmDecl>(D)->getIndex();
    } else if (isa<TemplateTypeParmDecl>(D)) {
      Index = (cast<TemplateTypeParmDecl>(D))->getIndex();
    } else return Error(R);

    const DeclContext *Parent = D->getDeclContext();
    if (isa<FunctionDecl>(Parent)) {
      const FunctionDecl *ParentFunction =
        cast<FunctionDecl>(Decl::castFromDeclContext(Parent));
      const FunctionTemplateDecl *FTD =
        ParentFunction->getDescribedFunctionTemplate();
      const TemplateParameterList *TPL = FTD->getTemplateParameters();
      if (++Index < TPL->size()) {
        return makeReflection(TPL->getParam(Index), Result);
      }
    } else if (isa<CXXRecordDecl>(Parent)) {
      // TODO: what about generic lambdas?
      const CXXRecordDecl *ParentRecord =
        cast<CXXRecordDecl>(Decl::castFromDeclContext(Parent));
      const ClassTemplateDecl *CTD = ParentRecord->getDescribedClassTemplate();
      const TemplateParameterList *TPL = CTD->getTemplateParameters();
      if (++Index < TPL->size()) {
        return makeReflection(TPL->getParam(Index), Result);
      }
    }

    return makeReflection(static_cast<Decl *>(nullptr), Result);
  }

  return Error(R);
}

/// Returns the first parameter of a function.
static bool getBeginParam(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R)) {
    const FunctionDecl *FD;
    if (isa<FunctionDecl>(D))
      FD = cast<FunctionDecl>(D);
    else if (isa<FunctionTemplateDecl>(D))
      FD = cast<FunctionTemplateDecl>(D)->getTemplatedDecl();
    else
      return Error(R);

    return makeReflection(getFirstFunctionParameter(FD), Result);
  }

  return getFunctionParameter(R, 0, Result);
}

/// Returns the next parameter of a function, if there is one.
static bool getNextParam(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R)) {
    if (const ParmVarDecl *PVD = cast<ParmVarDecl>(D))
      return makeReflection(getNextFunctionParameter(PVD), Result);
  }

  if (R.isType() && R.hasParent()) {
    Reflection Parent = R.getParent();

    // Note the offset stored is starts at 1, the offset used starts at 0
    // so no addition is required here.
    return getFunctionParameter(Parent, R.getOffsetInParent(), Result);
  }

  return Error(R);
}

// Returns the first member of a class.
static bool getBeginMember(const Reflection &R, APValue &Result) {
  if (const DeclContext *DC = getReachableDeclContext(R)) {
    const Decl *Member = findIterableMember(*DC->decls_begin());
    return makeReflection(Member, Result);
  }

  return Error(R);
}

// Returns the next member in a class.
static bool getNextMember(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableAliasDecl(R)) {
    const Decl *Member = findIterableMember(D->getNextDeclInContext());
    return makeReflection(Member, Result);
  }

  return Error(R);
}

static const CXXBaseSpecifier *
getBaseSpecifier(const CXXRecordDecl *RD, unsigned Index) {
  if (Index < RD->getNumBases())
    return (RD->bases_begin() + Index);

  return nullptr;
}

static bool
getBaseSpecifier(const Reflection &R, unsigned Index, APValue &Result) {
  if (const Decl *D = getReachableDecl(R)) {
    if (const ClassTemplateDecl *CTD = dyn_cast<ClassTemplateDecl>(D))
      D = CTD->getTemplatedDecl();

    if (const CXXRecordDecl *RD = dyn_cast<CXXRecordDecl>(D)) {
      APValue ParentRefl;
      if (!makeReflection(D, ParentRefl))
        llvm_unreachable("function type reflection creation failed");

      const CXXBaseSpecifier *Base = getBaseSpecifier(RD, Index);
      return makeReflection(Base, Index + 1, ParentRefl, Result);
    }
  }

  return Error(R);
}

// Returns the first base specifier of a class.
static bool getBeginBaseSpec(const Reflection &R, APValue &Result) {
  return getBaseSpecifier(R, 0, Result);
}

// Returns the next member in a class.
static bool getNextBaseSpec(const Reflection &R, APValue &Result) {
  if (R.isBase() && R.hasParent()) {
    Reflection Parent = R.getParent();

    // Note the offset stored is starts at 1, the offset used starts at 0
    // so no addition is required here.
    return getBaseSpecifier(Parent, R.getOffsetInParent(), Result);
  }

  return Error(R);
}

// [meta.trans.cv]p1:
// The member typedef type names the same type as T except that any top-level
// const-qualifier has been removed.
static bool removeConst(const Reflection &R, APValue &Result) {
  if (MaybeType MT = getCanonicalType(R)) {
    QualType T = *MT;
    if (T.isConstQualified()) {
      // Strip the const qualifier out of the qualifiers and rebuild the type.
      Qualifiers Quals = T.getQualifiers();
      Quals.removeConst();
      QualifierCollector QualCol(Quals);
      QualType NewType(T.getTypePtr(), 0);
      NewType = QualCol.apply(R.getContext(), NewType);
      return makeReflection(NewType, Result);
    }

    return makeReflection(T, Result);
  }

  return Error(R);
}

// [meta.trans.cv]p2:
// The member typedef type names the same type as T except that any top-level
// volatile-qualifier has been removed.
static bool removeVolatile(const Reflection &R, APValue &Result) {
  if (MaybeType MT = getCanonicalType(R)) {
    QualType T = *MT;
    if (T.isVolatileQualified()) {
      // Strip the volatile qualifier out of the
      // qualifiers and rebuild the type.
      Qualifiers Quals = T.getQualifiers();
      Quals.removeVolatile();
      QualifierCollector QualCol(Quals);
      QualType NewType(T.getTypePtr(), 0);
      NewType = QualCol.apply(R.getContext(), NewType);
      return makeReflection(NewType, Result);
    }

    return makeReflection(T, Result);
  }

  return Error(R);
}

// Equivalent to [meta.trans.cv]p4:
// If typename(R) = T is a reference, function, or top-level
// const-qualified type, then type names the same type as T, otherwise T const.
static bool addConst(const Reflection &R, APValue &Result) {
  if (MaybeType MT = getCanonicalType(R)) {
    QualType T = *MT;
    if (!T.isConstQualified() && !T->isFunctionType() && !T->isReferenceType())
      T.addConst();
    return makeReflection(T, Result);
  }

  return Error(R);
}

// Equivalent to [meta.trans.cv]p5:
// If typename(R) = T is a reference, function, or top-level
// volatile-qualified type, then type names the same type as T,
// otherwise T volatile.
static bool addVolatile(const Reflection &R, APValue &Result) {
  if (MaybeType MT = getCanonicalType(R)) {
    QualType T = *MT;
    if (!T.isVolatileQualified() && !T->isFunctionType()
        && !T->isReferenceType())
      T.addVolatile();
    return makeReflection(T, Result);
  }

  return Error(R);
}

// Equivalent to [meta.trans.ref]p1:
// If typename(R) = T has type “reference to T1” then the reflected type
// names T1; otherwise, type names T.
static bool removeReference(const Reflection &R, APValue &Result) {
  if (MaybeType MT = getCanonicalType(R)) {
    QualType T = *MT;
    if (T->isReferenceType())
      return makeReflection(T.getNonReferenceType(), Result);
    return makeReflection(T, Result);
  }

  return Error(R);
}

// Equivalent to [meta.trans.ref]p2:
// If typename(R) = T names a referenceable type then the reflected
// type names T&; otherwise, the reflected type names T.
// TODO: is reference collapsing implicit here?
static bool addLvalueReference(const Reflection &R, APValue &Result) {
  if (MaybeType MT = getCanonicalType(R)) {
    QualType T = *MT;
    QualType ReferenceableT = R.getContext().getLValueReferenceType(T);
    if (!ReferenceableT.isNull())
      return makeReflection(ReferenceableT, Result);
    return makeReflection(T, Result);
  }

  return Error(R);
}

// Equivalent to [meta.trans.ref]p3:
// If typename(R) = T names a referenceable type then the reflected
// type names T&&; otherwise, the reflected type names T.
// TODO: is reference collapsing implicit here?
static bool addRvalueReference(const Reflection &R, APValue &Result) {
  if (MaybeType MT = getCanonicalType(R)) {
    QualType T = *MT;
    QualType ReferenceableT = R.getContext().getRValueReferenceType(T);
    if (!ReferenceableT.isNull())
      return makeReflection(ReferenceableT, Result);
    return makeReflection(T, Result);
  }

  return Error(R);
}

// Equivalent [meta.trans.ptr]p1:
// If the reflected type names a type "array of U"
// then returns a reflection of type U.
//
// Otherwise returns the reflected type.
static bool removeExtent(const Reflection &R, APValue &Result) {
  if (MaybeType MT = getCanonicalType(R)) {
    if (MT->isArrayType())
      return makeReflection(cast<ArrayType>(*MT)->getElementType(), Result);
    return makeReflection(*MT, Result);
  }

  return Error(R);
}

// Equivalent [meta.trans.ptr]p1:
// If typename(R) = T has type “(possibly cv-qualified) pointer to T1” then the
// reflected type names T1; otherwise, it names T.
static bool removePointer(const Reflection &R, APValue &Result) {
  if (MaybeType MT = getCanonicalType(R)) {
    QualType T = MT->getPointeeType();
    if (!T.isNull())
      return makeReflection(T, Result);
    return makeReflection(*MT, Result);
  }

  return Error(R);
}

// Equivalent to [meta.trans.ptr]p2:
// If typename(R) = T names a referenceable type or a cv void type then
// the reflected type names the same type as remove_-reference(reflexpr(T))*;
// otherwise, the reflected type names T.
static bool addPointer(const Reflection &R, APValue &Result) {
  if (MaybeType MT = getCanonicalType(R)) {
    QualType T = R.getContext().getPointerType(*MT);
    if (!T.isNull())
      return makeReflection(T.getNonReferenceType(), Result);
    return makeReflection(*MT, Result);
  }

  return Error(R);
}

static QualType getSignedCorrespondent(const ASTContext &Ctx, const QualType &T)
{
  if (!T->isBuiltinType())
    return QualType();

  QualType Correspondent;
  switch (T->getAs<BuiltinType>()->getKind()) {
  case BuiltinType::UShort:
    Correspondent = QualType(Ctx.ShortTy.getTypePtr(), 0);
    break;
  case BuiltinType::UInt:
    Correspondent = QualType(Ctx.IntTy.getTypePtr(), 0);
    break;
  case BuiltinType::ULong:
    Correspondent = QualType(Ctx.LongTy.getTypePtr(), 0);
    break;
  case BuiltinType::ULongLong:
    Correspondent = QualType(Ctx.LongLongTy.getTypePtr(), 0);
    break;
  case BuiltinType::UInt128:
    Correspondent = QualType(Ctx.Int128Ty.getTypePtr(), 0);
    break;
  case BuiltinType::UChar:
    Correspondent = QualType(Ctx.SignedCharTy.getTypePtr(), 0);
    break;
  default: // silence warning
    break;
  }

  // We must maintain all the qualifiers from unsigned type.
  QualifierCollector Quals(T.getQualifiers());
  return Quals.apply(Ctx, Correspondent);
}

// Equivalent to [meta.trans.sign]p1:
// If typename(R) = T names a (possibly cv-qualified) signed integer type then
// the reflected type names the type T; otherwise, if T names a
// (possibly cv-qualified) unsigned integer type then the reflected type names
// the corresponding signed integer type, with the same cv-qualifiers as T;
// otherwise, type names the signed integer type with smallest rank for which
// sizeof(T) == sizeof(reflected type), with the same cv-qualifiers as T.
// Requires: T shall be a (possibly cv-qualified) integral type or enumeration
// but not a bool type.
static bool makeSigned(const Reflection &R, APValue &Result) {
  if (MaybeType MT = getCanonicalType(R)) {
    QualType T = *MT;

    if (T->isBooleanType())
      return Error(R);

    if (!T->isSignedIntegerType()) {
      QualType SignT;
      if (T->isFixedPointType())
        SignT = R.getContext().getCorrespondingSignedFixedPointType(T);
      else
        SignT = getSignedCorrespondent(R.getContext(), T);
      return makeReflection(SignT, Result);
    }
    return makeReflection(T, Result);
  }

  return Error(R);
}

// Equivalent to [meta.trans.sign]p2:
// If typename(R) = T names a (possibly cv-qualified) unsigned integer type
// then the reflected type names the type T; otherwise, if typename(R) names a
// (possibly cv-qualified) signed integer type then type names the corresponding
// unsigned integer type, with the same cv-qualifiers as T; otherwise,
// the reflected type names the unsigned integer type with smallest rank for
// which sizeof(T) == sizeof(type), with the same cv-qualifiers as T.
// Requires: typename(R) shall be a (possibly cv-qualified) integral type
// or enumeration but not a bool type.
static bool makeUnsigned(const Reflection &R, APValue &Result) {
  if (MaybeType MT = getCanonicalType(R)) {
    QualType T = *MT;
    if (T->isBooleanType())
      return Error(R);
    if (!T->isUnsignedIntegerType()) {
      QualType UnsT = R.getContext().getCorrespondingUnsignedType(T);
      return makeReflection(UnsT, Result);
    }
    return makeReflection(T, Result);
  }

  return Error(R);
}


bool Reflection::GetAssociatedReflection(ReflectionQuery Q, APValue &Result) {
  assert(isAssociatedReflectionQuery(Q) && "invalid query");
  switch (Q) {
  // Associated types
  case query_get_type:
    return getType(*this, Result);
  case query_get_return_type:
    return getReturnType(*this, Result);
  case query_get_this_ref_type:
    return getThisRefType(*this, Result);
  case query_get_underlying_type:
    return getUnderlyingType(*this, Result);

  // Entities
  case query_get_entity:
    return getEntity(*this, Result);
  case query_get_parent:
    return ::getParent(*this, Result);
  case query_get_definition:
    return getDefinition(*this, Result);

  // Traversal
  case query_get_begin:
    return getBegin(*this, Result);
  case query_get_next:
    return getNext(*this, Result);
  case query_get_begin_template_param:
    return getBeginTemplateParam(*this, Result);
  case query_get_next_template_param:
    return getNextTemplateParam(*this, Result);
  case query_get_begin_param:
    return getBeginParam(*this, Result);
  case query_get_next_param:
    return getNextParam(*this, Result);
  case query_get_begin_member:
    return getBeginMember(*this, Result);
  case query_get_next_member:
    return getNextMember(*this, Result);
  case query_get_begin_base_spec:
    return getBeginBaseSpec(*this, Result);
  case query_get_next_base_spec:
    return getNextBaseSpec(*this, Result);

  // Type transformation
  case query_remove_const:
    return removeConst(*this, Result);
  case query_remove_volatile:
    return removeVolatile(*this, Result);
  case query_add_const:
    return addConst(*this, Result);
  case query_add_volatile:
    return addVolatile(*this, Result);
  case query_remove_reference:
    return removeReference(*this, Result);
  case query_add_lvalue_reference:
    return addLvalueReference(*this, Result);
  case query_add_rvalue_reference:
    return addRvalueReference(*this, Result);
  case query_remove_extent:
    return removeExtent(*this, Result);
  case query_remove_pointer:
    return removePointer(*this, Result);
  case query_add_pointer:
    return addPointer(*this, Result);
  case query_make_signed:
    return makeSigned(*this, Result);
  case query_make_unsigned:
    return makeUnsigned(*this, Result);

  default:
    break;
  }
  llvm_unreachable("invalid reflection selector");
}

// Creates a c-string of type const char *.
//
// This is morally equivalent to creating a global string.
// During codegen, that's exactly how this is interpreted.
static Expr *MakeConstCharPointer(
    ASTContext& Ctx, StringRef Str, SourceLocation Loc) {
  QualType StrLitTy = Ctx.getConstantArrayType(
      Ctx.CharTy.withConst(), llvm::APInt(32, Str.size() + 1), nullptr,
      ArrayType::Normal, 0);

  // Create a string literal of type const char [L] where L
  // is the number of characters in the StringRef.
  StringLiteral *StrLit = StringLiteral::Create(
      Ctx, Str, StringLiteral::Ascii, false, StrLitTy, Loc);

  // Create an implicit cast expr so that we convert our const char [L]
  // into an actual const char * for proper evaluation.
  QualType StrTy = Ctx.getPointerType(Ctx.getConstType(Ctx.CharTy));
  return ImplicitCastExpr::Create(
      Ctx, StrTy, CK_ArrayToPointerDecay, StrLit, /*BasePath=*/nullptr,
      VK_RValue, FPOptionsOverride());
}

static bool getName(const Reflection R, APValue &Result) {
  ASTContext &Ctx = R.getContext();

  if (const Expr *NewNameExpr = R.getModifiers().getNewName()) {
    Expr::EvalResult Eval;
    Expr::EvalContext EvalCtx(Ctx, nullptr);
    if (!NewNameExpr->EvaluateAsConstantExpr(Eval, Expr::EvaluateForCodeGen,
                                             EvalCtx))
      return false;
    Result = Eval.Val;
    return true;
  }

  if (R.isType()) {
    QualType T = getQualType(R);

    // Render the string of the type.
    PrintingPolicy PP = Ctx.getPrintingPolicy();
    PP.SuppressTagKeyword = true;
    Expr *Str = MakeConstCharPointer(Ctx, T.getAsString(PP), SourceLocation());

    // Generate the result value.
    Expr::EvalResult Eval;
    Expr::EvalContext EvalCtx(Ctx, nullptr);
    if (!Str->EvaluateAsConstantExpr(Eval, Expr::EvaluateForCodeGen, EvalCtx))
      return false;
    Result = Eval.Val;
    return true;
  }

  if (const NamedDecl *ND = getReachableNamedAliasDecl(R)) {
    // Get the identifier of the declaration.
    Expr *Str = MakeConstCharPointer(Ctx, ND->getDeclName().getAsString(),
                                     SourceLocation());

    // Generate the result value.
    Expr::EvalResult Eval;
    Expr::EvalContext EvalCtx(Ctx, nullptr);
    if (!Str->EvaluateAsConstantExpr(Eval, Expr::EvaluateForCodeGen, EvalCtx))
      return false;
    Result = Eval.Val;
    return true;
  }

  return Error(R);
}

bool Reflection::GetName(ReflectionQuery Q, APValue &Result) {
  assert(isNameQuery(Q) && "invalid query");

  if (isInvalid()) {
    return Error(*this);
  }

  switch (Q) {
  // Names
  case query_get_name:
  case query_get_display_name:
    return getName(*this, Result);

  default:
    break;
  }

  llvm_unreachable("invalid name selector");
}

static ReflectionModifiers withAccess(const Reflection &R, std::uint64_t Val) {
  ReflectionModifiers M = R.getModifiers();
  M.setAccessModifier(static_cast<AccessModifier>(Val));
  return M;
}

static bool
setAccessMod(const Reflection &R, const ArrayRef<APValue> &Args,
             APValue &Result) {
  if (OptionalUInt V = getArgAsUInt(Args, 0))
    return makeReflection(R, withAccess(R, *V), Result);
  return Error(R);
}

static ReflectionModifiers withStorage(const Reflection &R, std::uint64_t Val) {
  ReflectionModifiers M = R.getModifiers();
  M.setStorageModifier(static_cast<StorageModifier>(Val));
  return M;
}

static bool
setStorageMod(const Reflection &R, const ArrayRef<APValue> &Args,
              APValue &Result) {
  if (OptionalUInt V = getArgAsUInt(Args, 0))
    return makeReflection(R, withStorage(R, *V), Result);
  return Error(R);
}

static ReflectionModifiers withConstexpr(const Reflection &R, std::uint64_t Val) {
  ReflectionModifiers M = R.getModifiers();
  M.setConstexprModifier(static_cast<ConstexprModifier>(Val));
  return M;
}

static bool
setConstexprMod(const Reflection &R, const ArrayRef<APValue> &Args,
                APValue &Result) {
  if (OptionalUInt V = getArgAsUInt(Args, 0))
    return makeReflection(R, withConstexpr(R, *V), Result);
  return Error(R);
}

static ReflectionModifiers withExplicit(const Reflection &R, bool AddExplicit) {
  ReflectionModifiers M = R.getModifiers();
  M.setAddExplicit(AddExplicit);
  return M;
}

static bool
setAddExplicitMod(const Reflection &R, const ArrayRef<APValue> &Args,
                   APValue &Result) {
  if (OptionalBool V = getArgAsBool(Args, 0))
    return makeReflection(R, withExplicit(R, *V), Result);
  return Error(R);
}

static ReflectionModifiers withVirtual(const Reflection &R, bool AddVirtual) {
  ReflectionModifiers M = R.getModifiers();
  M.setAddVirtual(AddVirtual);
  return M;
}

static bool
setAddVirtualMod(const Reflection &R, const ArrayRef<APValue> &Args,
                 APValue &Result) {
  if (OptionalBool V = getArgAsBool(Args, 0))
    return makeReflection(R, withVirtual(R, *V), Result);
  return Error(R);
}

static ReflectionModifiers withPureVirtual(const Reflection &R,
                                           bool AddPureVirtual) {
  ReflectionModifiers M = R.getModifiers();
  M.setAddPureVirtual(AddPureVirtual);
  return M;
}

static bool
setAddPureVirtualMod(const Reflection &R, const ArrayRef<APValue> &Args,
                     APValue &Result) {
  if (OptionalBool V = getArgAsBool(Args, 0))
    return makeReflection(R, withPureVirtual(R, *V), Result);
  return Error(R);
}

static ReflectionModifiers withInline(const Reflection &R, bool AddInline) {
  ReflectionModifiers M = R.getModifiers();
  M.setAddInline(AddInline);
  return M;
}

static bool
setAddInlineMod(const Reflection &R, const ArrayRef<APValue> &Args,
                APValue &Result) {
  if (OptionalBool V = getArgAsBool(Args, 0))
    return makeReflection(R, withInline(R, *V), Result);
  return Error(R);
}

static ReflectionModifiers withNewName(const Reflection &R, const Expr *NewName) {
  ReflectionModifiers M = R.getModifiers();
  M.setNewName(NewName);
  return M;
}

static bool
setNewNameMod(const Reflection &R, const ArrayRef<APValue> &Args,
              APValue &Result) {
  if (OptionalExpr V = getArgAsExpr(Args, 0))
    return makeReflection(R, withNewName(R, *V), Result);
  return Error(R);
}

bool Reflection::UpdateModifier(ReflectionQuery Q,
                                const ArrayRef<APValue> &ContextualArgs,
                                APValue &Result) {
  assert(isModifierUpdateQuery(Q) && "invalid query");

  switch(Q) {
  case query_set_access:
    return setAccessMod(*this, ContextualArgs, Result);
  case query_set_storage:
    return setStorageMod(*this, ContextualArgs, Result);
  case query_set_constexpr:
    return setConstexprMod(*this, ContextualArgs, Result);
  case query_set_add_explicit:
    return setAddExplicitMod(*this, ContextualArgs, Result);
  case query_set_add_virtual:
    return setAddVirtualMod(*this, ContextualArgs, Result);
  case query_set_add_pure_virtual:
    return setAddPureVirtualMod(*this, ContextualArgs, Result);
  case query_set_add_inline:
    return setAddInlineMod(*this, ContextualArgs, Result);
  case query_set_new_name:
    return setNewNameMod(*this, ContextualArgs, Result);
  default:
    break;
  }

  llvm_unreachable("invalid modifier update query");
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
                      getQualType(A),
                      getQualType(B));
  case RK_declaration:
    return EqualDecls(A.getReflectedDeclaration(),
                      B.getReflectedDeclaration());
  default:
    return false;
  }
}

namespace clang {

bool EvaluateReflection(Sema &S, Expr *E, Reflection &R) {
  SmallVector<PartialDiagnosticAt, 4> Diags;
  Expr::EvalResult Result;
  Result.Diag = &Diags;
  Expr::EvalContext EvalCtx(S.Context, S.GetReflectionCallbackObj());
  if (!E->EvaluateAsRValue(Result, EvalCtx)) {
    S.Diag(E->getExprLoc(), diag::err_reflection_not_constant_expression);
    for (PartialDiagnosticAt PD : Diags)
      S.Diag(PD.first, PD.second);
    return true;
  }

  R = Reflection(S.Context, Result.Val);
  return false;
}

void DiagnoseInvalidReflection(Sema &SemaRef, Expr *E, const Reflection &R) {
  SemaRef.Diag(E->getExprLoc(), diag::err_reify_invalid_reflection);

  const InvalidReflection *InvalidRefl = R.getAsInvalidReflection();
  if (!InvalidRefl)
    return;

  const Expr *ErrorMessage = InvalidRefl->ErrorMessage;
  const StringLiteral *Message = cast<StringLiteral>(ErrorMessage);

  // Evaluate the message so that we can transform it into a string.
  SmallString<256> Buf;
  llvm::raw_svector_ostream OS(Buf);
  Message->outputString(OS);
  std::string NonQuote(Buf.c_str(), 1, Buf.size() - 2);

  SemaRef.Diag(E->getExprLoc(), diag::note_user_defined_note) << NonQuote;
}

}
