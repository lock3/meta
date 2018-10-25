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
#include "clang/AST/ExprCXX.h"
using namespace clang;

/// Returns the TypeDecl for a reflected Type, if any.
static const TypeDecl *getAsTypeDecl(const Reflection& R) {
  if (R.isType()) {
    QualType T = R.getAsType();
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
static const Decl *getReachableDecl(const Reflection& R) {
  if (const TypeDecl *TD = getAsTypeDecl(R))
    return TD;
  if (R.isDeclaration())
    return R.getAsDeclaration();
  if (R.isExpression())
    return getEntityDecl(R.getAsExpression());
  return nullptr;
}

/// Returns true if R is an entity.
static bool isEntity(const Reflection& R) {
  if (R.isType())
    return true;
  if (R.isDeclaration())
    return isa<ValueDecl>(R.getAsDeclaration());
  return false;
}

/// Returns true if R is unnamed.
static bool isUnnamed(const Reflection& R) {
  if (const Decl *D = R.getAsDeclaration()) {
    if (const NamedDecl *ND = dyn_cast<NamedDecl>(D))
      return ND->getIdentifier() == nullptr;
  }
  // FIXME: Diagnose the error, don't fail.
  assert(false);
}

/// Returns true if R designates a variable.
static bool isVariable(const Reflection& R) {
  if (const Decl *D = getReachableDecl(R))
    return isa<VarDecl>(D);
  return false;
}

/// Returns true if R designates an enumerator.
static bool isEnumerator(const Reflection& R) {
  if (const Decl *D = getReachableDecl(R))
    return isa<EnumConstantDecl>(D);
  return false;
}

/// Returns the reflected member function.
static const CXXMethodDecl *getAsMemberFunction(const Reflection& R) {
  if (const Decl *D = getReachableDecl(R))
    return dyn_cast<CXXMethodDecl>(D);
  return nullptr;
}

/// Returns true if R designates a static member function
static bool isStaticMemberFunction(const Reflection& R) {
  if (const CXXMethodDecl *M = getAsMemberFunction(R))
    return M->isStatic();
  return false;
}

/// Returns true if R designates a nonstatic member function
static bool isNonstaticMemberFunction(const Reflection& R) {
  if (const CXXMethodDecl *M = getAsMemberFunction(R))
    return M->isInstance();
  return false;
}

/// Returns the reflected data member.
static const FieldDecl *getAsDataMember(const Reflection& R) {
  if (const Decl *D = getReachableDecl(R))
    return dyn_cast<FieldDecl>(D);
  return nullptr;
}

/// Returns true if this a static member variable.
static bool isStaticDataMember(const Reflection& R) {
  if (const Decl *D = getReachableDecl(R)) {
    if (const VarDecl *Var = dyn_cast<VarDecl>(D))
      return Var->isStaticDataMember();
  }
  return false;
}

/// Returns true if R designates a nonstatic data member.
static bool isNonstaticDataMember(const Reflection& R) {
  if (const FieldDecl *D = getAsDataMember(R))
    // FIXME: Is a bitfield a nsdm?
    return true;
  return false;
}

/// Returns true if R designates a nonstatic data member.
static bool isBitField(const Reflection& R) {
  if (const FieldDecl *D = getAsDataMember(R))
    return D->isBitField();
  return false;
}

/// Returns true if R designates an constructor.
static bool isConstructor(const Reflection& R) {
  if (const Decl *D = getReachableDecl(R))
    return isa<CXXConstructorDecl>(D);
  return false;
}

/// Returns true if R designates an enumerator.
static bool isDestructor(const Reflection& R) {
  if (const Decl *D = getReachableDecl(R))
    return isa<CXXDestructorDecl>(D);
  return false;
}

/// Returns an APValue-packaged truth value.
static APValue MakeBool(ASTContext& C, bool B) {
  return APValue(C.MakeIntValue(B, C.BoolTy));
}

APValue Reflection::EvaluatePredicate(ReflectionQuery Q) {
  assert(isPredicateQuery(Q));
  switch (Q) {
  case RQ_is_invalid:
    return MakeBool(Ctx, isInvalid());
  case RQ_is_entity:
    return MakeBool(Ctx, isEntity(*this));
  case RQ_is_unnamed:
    return MakeBool(Ctx, isUnnamed(*this));

  case RQ_is_variable:
    return MakeBool(Ctx, isVariable(*this));
  case RQ_is_enumerator:
    return MakeBool(Ctx, isEnumerator(*this));
  case RQ_is_static_data_member:
    return MakeBool(Ctx, isStaticDataMember(*this));
  case RQ_is_static_member_function:
    return MakeBool(Ctx, isStaticMemberFunction(*this));
  case RQ_is_nonstatic_data_member:
    return MakeBool(Ctx, isNonstaticDataMember(*this));
  case RQ_is_bitfield:
    return MakeBool(Ctx, isBitField(*this));
  case RQ_is_nonstatic_member_function:
    return MakeBool(Ctx, isNonstaticMemberFunction(*this));
  case RQ_is_constructor:
    return MakeBool(Ctx, isConstructor(*this));
  case RQ_is_destructor:
    return MakeBool(Ctx, isDestructor(*this));
  
  case RQ_is_type:
  case RQ_is_class:
  case RQ_is_union:
  case RQ_is_enum:
  case RQ_is_scoped_enum:
  case RQ_is_void:
  case RQ_is_null_pointer:
  case RQ_is_integral:
  case RQ_is_floating_point:
  case RQ_is_array:
  case RQ_is_pointer:
  case RQ_is_lvalue_reference:
  case RQ_is_rvalue_reference:
  case RQ_is_member_object_pointer:
  case RQ_is_member_function_pointer:
  case RQ_is_closure:

  case RQ_is_namespace:
  case RQ_is_namespace_alias:
  case RQ_is_type_alias:

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
  case RQ_is_lvalue:
  case RQ_is_xvalue:
  case RQ_is_rvalue:

  case RQ_is_local:
  case RQ_is_class_member:

  default:
    break;
  }
  llvm_unreachable("invalid predicate selector");
}

APValue Reflection::GetTraits(ReflectionQuery Q) {
  return {};
}

APValue Reflection::GetAssociatedReflection(ReflectionQuery Q) {
  return {};
}

APValue Reflection::GetName(ReflectionQuery) {
  return {};
}

