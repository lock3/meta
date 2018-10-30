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
static APValue MakeBool(ASTContext &C, bool B) {
  return APValue(C.MakeIntValue(B, C.BoolTy));
}

/// Sets result to the truth value of B and returns true.
static bool SuccessIf(const Reflection &R, APValue &Result, bool B) {
  Result = MakeBool(R.Ctx, B);
  return true;
}

static bool SuccessTrue(const Reflection &R, APValue &Result) {
  return SuccessIf(R, Result, true);
}

static bool SuccessFalse(const Reflection &R, APValue &Result) {
  return SuccessIf(R, Result, false);
}

// Returns false, possibly saving the diagnostic.
static bool Error(const Reflection &R, 
                  diag::kind Diag = diag::note_reflection_not_defined) {
  if (R.Diag) {
    // FIXME: We could probably do a better job with the location.
    SourceLocation Loc = R.Query->getExprLoc();
    PartialDiagnostic PD(Diag, R.Ctx.getDiagAllocator());
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
struct MaybeType
{
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
  return R.Ctx.getCanonicalType(T);
}

static bool isInvalid(const Reflection &R, APValue &Result) {
  return SuccessIf(R, Result, R.isInvalid());
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
      return SuccessIf(R, Result, !isa<TemplateTemplateParmDecl>(D));
    
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
      return SuccessIf(R, Result, ND->getIdentifier() == nullptr);
  }
  return Error(R);
}

/// Returns true if R designates a variable.
static bool isVariable(const Reflection &R, APValue& Result) {
  if (const Decl *D = getReachableDecl(R))
    return SuccessIf(R, Result, isa<VarDecl>(D));
  return SuccessFalse(R, Result);
}

/// Returns true if R designates an enumerator.
static bool isEnumerator(const Reflection &R, APValue& Result) {
  if (const Decl *D = getReachableDecl(R))
    return SuccessIf(R, Result, isa<EnumConstantDecl>(D));
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
    return SuccessIf(R, Result, M->isStatic());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates a nonstatic member function
static bool isNonstaticMemberFunction(const Reflection &R, APValue &Result) {
  if (const CXXMethodDecl *M = getAsMemberFunction(R))
    return SuccessIf(R, Result, M->isInstance());
  return SuccessFalse(R, Result);
}

/// Returns the reflected data member.
static const FieldDecl *getAsDataMember(const Reflection &R) {
  if (const Decl *D = getReachableDecl(R))
    return dyn_cast<FieldDecl>(D);
  return nullptr;
}

/// Returns true if this a static member variable.
static bool isStaticDataMember(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R)) {
    if (const VarDecl *Var = dyn_cast<VarDecl>(D))
      return SuccessIf(R, Result, Var->isStaticDataMember());
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
    return SuccessIf(R, Result, D->isBitField());
  return SuccessFalse(R, Result);
}

/// Returns true if R designates an constructor.
static bool isConstructor(const Reflection &R,APValue &Result) {
  if (const Decl *D = getReachableDecl(R))
    return SuccessIf(R, Result, isa<CXXConstructorDecl>(D));
  return SuccessFalse(R, Result);
}

/// Returns true if R designates an enumerator.
static bool isDestructor(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R))
    return SuccessIf(R, Result, isa<CXXDestructorDecl>(D));
  return SuccessFalse(R, Result);
}

static bool isType(const Reflection &R, APValue &Result) {
  if (MaybeType T = getReachableType(R))
    return SuccessTrue(R, Result);
  return SuccessFalse(R, Result);
}

static bool isClass(const Reflection &R, APValue &Result) {
  if (MaybeType T = getReachableCanonicalType(R)) {
    return SuccessIf(R, Result, T->isRecordType());
  }
  return SuccessFalse(R, Result);
}

static bool isUnion(const Reflection &R, APValue &Result) {
  if (MaybeType T = getReachableCanonicalType(R))
    return SuccessIf(R, Result, T->isUnionType());
  return SuccessFalse(R, Result);
}

static bool isEnum(const Reflection &R, APValue &Result) {
  if (MaybeType T = getReachableCanonicalType(R))
    return SuccessIf(R, Result, T->isEnumeralType());
  return SuccessFalse(R, Result);
}

static bool isScopedEnum(const Reflection &R, APValue &Result) {
  if (MaybeType T = getReachableCanonicalType(R))
    return SuccessIf(R, Result, T->isScopedEnumeralType());
  return SuccessFalse(R, Result);
}

static bool isTypeAlias(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R))
    return SuccessIf(R, Result, isa<TypedefNameDecl>(D));
  return SuccessFalse(R, Result);
}

static bool isNamespace(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R))
    return SuccessIf(R, Result, isa<NamespaceDecl>(D));
  return SuccessFalse(R, Result);
}

static bool isNamespaceAlias(const Reflection &R, APValue &Result) {
  if (const Decl *D = getReachableDecl(R))
    return SuccessIf(R, Result, isa<NamespaceAliasDecl>(D));
  return SuccessFalse(R, Result);
}

static bool isExpression(const Reflection &R, APValue &Result) {
  return SuccessIf(R, Result, R.isExpression());
}

bool Reflection::EvaluatePredicate(ReflectionQuery Q, APValue &Result) {
  assert(isPredicateQuery(Q));
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
  case RQ_is_class:
    return isClass(*this, Result);
  case RQ_is_union:
    return isUnion(*this, Result);
  case RQ_is_enum:
    return isEnum(*this, Result);
  case RQ_is_scoped_enum:
    return isScopedEnum(*this, Result);
  
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

bool Reflection::GetTraits(ReflectionQuery Q, APValue& Result) {
  return {};
}

bool Reflection::GetAssociatedReflection(ReflectionQuery Q, APValue& Result) {
  return {};
}

bool Reflection::GetName(ReflectionQuery Q, APValue& Result) {
  return {};
}

