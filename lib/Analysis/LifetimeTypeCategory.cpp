//=- LifetimeTypeCategory.cpp - Diagnose lifetime violations -*- C++ -*-======//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/Analysis/Analyses/LifetimeTypeCategory.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"

namespace clang {
namespace lifetime {

template <typename T>
static bool hasMethodLike(const CXXRecordDecl *R, T Predicate) {
  // TODO cache IdentifierInfo to avoid string compare
  auto CallBack = [Predicate](const CXXRecordDecl *Base) {
    return std::none_of(
        Base->method_begin(), Base->method_end(),
        [Predicate](const CXXMethodDecl *M) { return Predicate(M); });
  };
  return !R->forallBases(CallBack) || !CallBack(R);
}

static bool hasMethodWithNameAndArgNum(const CXXRecordDecl *R, StringRef Name,
                                       int ArgNum = -1) {
  return hasMethodLike(R, [Name, ArgNum](const CXXMethodDecl *M) {
    if (ArgNum >= 0 && (unsigned)ArgNum != M->getMinRequiredArguments())
      return false;
    auto *I = M->getDeclName().getAsIdentifierInfo();
    if (!I)
      return false;
    return I->getName() == Name;
  });
}

static bool satisfiesContainerRequirements(const CXXRecordDecl *R) {
  // TODO https://en.cppreference.com/w/cpp/named_req/Container
  return hasMethodWithNameAndArgNum(R, "begin", 0) &&
         hasMethodWithNameAndArgNum(R, "end", 0) && !R->hasTrivialDestructor();
}

static bool satisfiesIteratorRequirements(const CXXRecordDecl *R) {
  // TODO https://en.cppreference.com/w/cpp/named_req/Iterator
  bool hasDeref = false;
  bool hasPlusPlus = false;
  // TODO: check base classes.
  for (auto *M : R->methods()) {
    auto O = M->getDeclName().getCXXOverloadedOperator();
    if (O == OO_PlusPlus)
      hasPlusPlus = true;
    else if (O == OO_Star && M->param_empty())
      hasDeref = true;
    if (hasPlusPlus && hasDeref)
      return true;
  }
  return false;
}

static bool satisfiesRangeConcept(const CXXRecordDecl *R) {
  // TODO https://en.cppreference.com/w/cpp/experimental/ranges/range/Range
  return hasMethodWithNameAndArgNum(R, "begin", 0) &&
         hasMethodWithNameAndArgNum(R, "end", 0) && R->hasTrivialDestructor();
}

/// Classifies some well-known std:: types or returns an empty optional.
/// Checks the type both before and after desugaring.
// TODO:
// Unfortunately, the types are stored in a desugared form for template
// instantiations. For this and some other reasons I think it would be better
// to look up the declarations (pointers) by names upfront and look up the
// declarations instead of matching strings.
static Optional<TypeCategory> classifyStd(const Type *T) {
  NamedDecl *Decl;
  if (const auto *TypeDef = T->getAs<TypedefType>()) {
    if (auto TypeCat = classifyStd(TypeDef->desugar().getTypePtr()))
      return TypeCat;
    Decl = TypeDef->getDecl();
  } else
    Decl = T->getAsCXXRecordDecl();
  if (!Decl || !Decl->isInStdNamespace() || !Decl->getIdentifier())
    return {};

  static std::set<StringRef> StdOwners{"stack",    "queue",   "priority_queue",
                                       "optional", "variant", "any"};
  static std::set<StringRef> StdPointers{"basic_regex", "reference_wrapper",
                                         "vector<bool>::reference"};

  if (StdOwners.count(Decl->getName()))
    return TypeCategory::Owner;
  if (StdPointers.count(Decl->getName()))
    return TypeCategory::Pointer;

  return {};
}

/// Returns the type category of the given type
/// If T is a template specialization, it must be instantiated.
TypeCategory classifyTypeCategory(QualType QT) {
  /*
          llvm::errs() << "classifyTypeCategory\n ";
           T->dump(llvm::errs());
           llvm::errs() << "\n";*/

  const Type *T = QT.getUnqualifiedType().getTypePtr();
  const auto *R = T->getAsCXXRecordDecl();

  if (!R) {
    // raw pointers and references
    // Arrays are Pointers, because they implicitly convert into them
    // and we don't track implicit conversions.
    if (T->isArrayType() || T->isPointerType() || T->isReferenceType())
      return TypeCategory::Pointer;

    return TypeCategory::Value;
  }

  assert(R->hasDefinition());

  if (R->hasAttr<OwnerAttr>())
    return TypeCategory::Owner;

  if (R->hasAttr<PointerAttr>())
    return TypeCategory::Pointer;

  // Every type that satisfies the standard Container requirements.
  if (satisfiesContainerRequirements(R))
    return TypeCategory::Owner;

  // TODO: handle outside class definition 'R& operator*(T a);' .
  bool hasDerefOperations = hasMethodLike(R, [](const CXXMethodDecl *M) {
    auto O = M->getDeclName().getCXXOverloadedOperator();
    return (O == OO_Arrow) || (O == OO_Star && M->param_empty());
  });

  // Every type that provides unary * or -> and has a user-provided destructor.
  // (Example: unique_ptr.)
  if (hasDerefOperations && R->hasUserDeclaredDestructor())
    return TypeCategory::Owner;

  if (auto Cat = classifyStd(T))
    return *Cat;

  //  Every type that satisfies the Ranges TS Range concept.
  if (satisfiesRangeConcept(R))
    return TypeCategory::Pointer;

  // Every type that satisfies the standard Iterator requirements. (Example:
  // regex_iterator.), see https://en.cppreference.com/w/cpp/named_req/Iterator
  if (satisfiesIteratorRequirements(R))
    return TypeCategory::Pointer;

  // Every type that provides unary * or -> and does not have a user-provided
  // destructor. (Example: span.)
  if (hasDerefOperations && !R->hasUserDeclaredDestructor())
    return TypeCategory::Pointer;

  // Every closure type of a lambda that captures by reference.
  if (R->isLambda() &&
      std::any_of(R->field_begin(), R->field_end(), [](const FieldDecl *FD) {
        return FD->getType()->isReferenceType();
      })) {
    return TypeCategory::Pointer;
  }

  // An Aggregate is a type that is not an Indirection
  // and is a class type with public data members
  // and no user-provided copy or move operations.
  if (R->isAggregate())
    return TypeCategory::Aggregate;

  // A Value is a type that is neither an Indirection nor an Aggregate.
  return TypeCategory::Value;
}

// TODO: check gsl namespace?
// TODO: handle clang nullability annotations?
bool isNullableType(QualType QT) {
  auto getKnownNullability = [](StringRef Name) -> Optional<bool> {
    if (Name == "nullable")
      return true;
    if (Name == "not_null")
      return false;
    return {};
  };
  QualType Inner = QT;
  if (const auto *TemplSpec = Inner->getAs<TemplateSpecializationType>()) {
    if (TemplSpec->isTypeAlias()) {
      if (const auto *TD = TemplSpec->getTemplateName().getAsTemplateDecl()) {
        if (TD->getIdentifier()) {
          if (auto Nullability = getKnownNullability(TD->getName()))
            return *Nullability;
        }
      }
    }
  }
  while (const auto *TypeDef = Inner->getAs<TypedefType>()) {
    const NamedDecl *Decl = TypeDef->getDecl();
    if (auto Nullability = getKnownNullability(Decl->getName()))
      return *Nullability;
    Inner = TypeDef->desugar();
  }
  if (const auto *RD = Inner->getAsCXXRecordDecl()) {
    if (auto Nullability = getKnownNullability(RD->getName()))
      return *Nullability;
  }
  return QT.getCanonicalType()->isPointerType();
}

// For primitive types like pointers, references we return the pointee.
// For user defined types the pointee type is determined by the return
// type of operator*, operator-> or operator[]. Since these methods
// might return references, and operator-> returns a pointers, we strip
// off one extra level of pointers sometimes.
QualType getPointeeType(QualType QT) {
  if (QT->isReferenceType() || QT->isAnyPointerType())
    return QT->getPointeeType();

  if (const auto *R = QT->getAsCXXRecordDecl()) {
    for (auto *M : R->methods()) {
      auto O = M->getDeclName().getCXXOverloadedOperator();
      if (O == OO_Arrow || O == OO_Star || O == OO_Subscript) {
        QT = M->getReturnType();
        if (QT->isReferenceType() || QT->isAnyPointerType())
          return QT->getPointeeType();
        return QT;
      }
    }
  }
  return {};
}
} // namespace lifetime
} // namespace clang