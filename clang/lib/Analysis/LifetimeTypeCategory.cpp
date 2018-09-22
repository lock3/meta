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
#include <map>

#define CLASSIFY_DEBUG 0

namespace clang {
namespace lifetime {

static FunctionDecl *lookupOperator(const CXXRecordDecl *R,
                                    OverloadedOperatorKind Op) {
  return GlobalLookupOperator(R, Op);
}

static FunctionDecl *lookupMemberFunction(const CXXRecordDecl *R,
                                          StringRef Name) {
  return GlobalLookupMemberFunction(R, Name);
}

template <typename T>
static bool hasMethodLike(const CXXRecordDecl *R, T Predicate) {
  // TODO cache IdentifierInfo to avoid string compare
  auto CallBack = [Predicate](const CXXRecordDecl *Base) {
    return std::none_of(
        Base->decls_begin(), Base->decls_end(), [Predicate](const Decl *D) {
          if (auto *M = dyn_cast<CXXMethodDecl>(D))
            return Predicate(M);
          if (auto *Tmpl = dyn_cast<FunctionTemplateDecl>(D)) {
            if (auto *M = dyn_cast<CXXMethodDecl>(Tmpl->getTemplatedDecl()))
              return Predicate(M);
          }
          return false;
        });
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

static bool hasDerefOperations(const CXXRecordDecl *R) {
  return lookupOperator(R, OO_Arrow) || lookupOperator(R, OO_Star);
}

/// Determines if D is std::vector<bool>::reference
static bool IsVectorBoolReference(const CXXRecordDecl *D) {
  assert(D);
  static std::set<StringRef> StdVectorBoolReference{
      "__bit_reference" /* for libc++ */, "_Bit_reference" /* for libstdc++ */,
      "_Vb_reference" /* for MSVC */};
  if (!D->isInStdNamespace() || !D->getIdentifier())
    return false;
  return StdVectorBoolReference.count(D->getName());
}

/// Classifies some well-known std:: types or returns an empty optional.
/// Checks the type both before and after desugaring.
// TODO:
// Unfortunately, the types are stored in a desugared form for template
// instantiations. For this and some other reasons I think it would be better
// to look up the declarations (pointers) by names upfront and look up the
// declarations instead of matching strings populated lazily.
static Optional<TypeCategory> classifyStd(const Type *T) {
  // MSVC: _Ptr_base is a base class of shared_ptr, and we only see
  // _Ptr_base when calling get() on a shared_ptr.
  static std::set<StringRef> StdOwners{"stack", "queue", "priority_queue",
                                       "optional", "_Ptr_base"};
  static std::set<StringRef> StdPointers{"basic_regex", "reference_wrapper"};
  auto *Decl = T->getAsCXXRecordDecl();
  if (!Decl || !Decl->isInStdNamespace() || !Decl->getIdentifier())
    return {};

  if (StdOwners.count(Decl->getName()))
    return TypeCategory::Owner;
  if (StdPointers.count(Decl->getName()))
    return TypeCategory::Pointer;
  if (IsVectorBoolReference(Decl))
    return TypeCategory::Pointer;

  return {};
}

static TypeClassification classifyTypeCategoryImpl(const Type *T) {
  assert(T);
  auto *R = T->getAsCXXRecordDecl();

  if (!R) {
    if (T->isVoidPointerType())
      return TypeCategory::Value;

    // raw pointers and references
    // Arrays are Pointers, because they implicitly convert into them
    // and we don't track implicit conversions.
    if (T->isArrayType() || T->isPointerType() || T->isReferenceType())
      return {TypeCategory::Pointer, getPointeeType(T)};

    return TypeCategory::Value;
  }

  if (!R->hasDefinition()) {
    if (auto *CDS = dyn_cast<ClassTemplateSpecializationDecl>(R))
      GlobalDefineClassTemplateSpecialization(CDS);
  }

  if (!R->hasDefinition())
    return TypeCategory::Value;

  // In case we do not know the pointee type fall back to value.
  QualType Pointee = getPointeeType(T);

#if CLASSIFY_DEBUG
  llvm::errs() << "classifyTypeCategory " << QualType(T, 0).getAsString()
               << "\n";
  llvm::errs() << "  satisfiesContainerRequirements(R): "
               << satisfiesContainerRequirements(R) << "\n";
  llvm::errs() << "  hasDerefOperations(R): " << hasDerefOperations(R) << "\n";
  llvm::errs() << "  satisfiesRangeConcept(R): " << satisfiesRangeConcept(R)
               << "\n";
  llvm::errs() << "  hasTrivialDestructor(R): " << R->hasTrivialDestructor()
               << "\n";
  llvm::errs() << "  satisfiesIteratorRequirements(R): "
               << satisfiesIteratorRequirements(R) << "\n";
  llvm::errs() << "  R->isAggregate(): " << R->isAggregate() << "\n";
  llvm::errs() << "  R->isLambda(): " << R->isLambda() << "\n";
  llvm::errs() << "  DerefType: " << Pointee.getAsString() << "\n";
#endif

  if (R->hasAttr<OwnerAttr>()) {
    if (Pointee.isNull())
      return TypeCategory::Value; // TODO diagnose
    else
      return {TypeCategory::Owner, Pointee};
  }

  if (R->hasAttr<PointerAttr>()) {
    if (Pointee.isNull())
      return TypeCategory::Value; // TODO diagnose
    else
      return {TypeCategory::Pointer, Pointee};
  }

  // Every type that satisfies the standard Container requirements.
  if (!Pointee.isNull() && satisfiesContainerRequirements(R))
    return {TypeCategory::Owner, Pointee};

  // Every type that provides unary * or -> and has a user-provided destructor.
  // (Example: unique_ptr.)
  if (!Pointee.isNull() && hasDerefOperations(R) && !R->hasTrivialDestructor())
    return {TypeCategory::Owner, Pointee};

  if (auto Cat = classifyStd(T))
    return {*Cat, Pointee};

  //  Every type that satisfies the Ranges TS Range concept.
  if (!Pointee.isNull() && satisfiesRangeConcept(R))
    return {TypeCategory::Pointer, Pointee};

  // Every type that satisfies the standard Iterator requirements. (Example:
  // regex_iterator.), see https://en.cppreference.com/w/cpp/named_req/Iterator
  if (!Pointee.isNull() && satisfiesIteratorRequirements(R))
    return {TypeCategory::Pointer, Pointee};

  // Every type that provides unary * or -> and does not have a user-provided
  // destructor. (Example: span.)
  if (!Pointee.isNull() && hasDerefOperations(R) && R->hasTrivialDestructor())
    return {TypeCategory::Pointer, Pointee};

  // Every closure type of a lambda that captures by reference.
  if (R->isLambda()) {
    return TypeCategory::Value;
  }

  // An Aggregate is a type that is not an Indirection
  // and is a class type with public data members
  // and no user-provided copy or move operations.
  if (R->isAggregate())
    return TypeCategory::Aggregate;

  // A Value is a type that is neither an Indirection nor an Aggregate.
  return TypeCategory::Value;
}

TypeClassification classifyTypeCategory(const Type *T) {
  static std::map<const Type *, TypeClassification> Cache;

  auto I = Cache.find(T);
  if (I != Cache.end())
    return I->second;

  auto TC = classifyTypeCategoryImpl(T);
  Cache.emplace(T, TC);
#if CLASSIFY_DEBUG
  llvm::errs() << "classifyTypeCategory(" << QualType(T, 0).getAsString()
               << ") = " << TC.str() << "\n";
#endif
  return TC;
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
  return classifyTypeCategory(QT) == TypeCategory::Pointer &&
         !QT->isReferenceType();
}

static QualType getPointeeType(const CXXRecordDecl *R) {
  assert(R);

  for (auto Op : {OO_Star, OO_Arrow, OO_Subscript}) {
    if (auto *F = lookupOperator(R, Op)) {
      auto PointeeType = F->getReturnType();
      if (PointeeType->isReferenceType() || PointeeType->isAnyPointerType())
        PointeeType = PointeeType->getPointeeType();
      return PointeeType.getCanonicalType();
    }
  }

  if (auto *F = lookupMemberFunction(R, "begin")) {
    auto PointeeType = F->getReturnType();
    if (classifyTypeCategory(PointeeType) != TypeCategory::Pointer) {
#if CLASSIFY_DEBUG
      // TODO: diag?
      llvm::errs() << "begin() function does not return a Pointer!\n";
      F->dump();
#endif
      return {};
    }
    PointeeType = getPointeeType(PointeeType);
    return PointeeType.getCanonicalType();
  }

  return {};
}

static QualType getPointeeTypeImpl(const Type *T) {
  // llvm::errs() << "\n\ngetPointeeType " << QT.getAsString() << " asRecord:"
  // << (intptr_t)QT->getAsCXXRecordDecl() << "\n";

  if (T->isReferenceType() || T->isAnyPointerType())
    return T->getPointeeType();

  if (T->isArrayType()) {
    // TODO: use AstContext.getAsArrayType() to correctly promote qualifiers
    auto *AT = dyn_cast<ArrayType>(T);
    return AT->getElementType();
  }

  auto *R = T->getAsCXXRecordDecl();
  if (!R)
    return {};

  // std::vector<bool> contains std::vector<bool>::references
  if (IsVectorBoolReference(R))
    return QualType(T, 0);

  if (!R->hasDefinition()) {
    if (auto *CDS = dyn_cast<ClassTemplateSpecializationDecl>(R))
      GlobalDefineClassTemplateSpecialization(CDS);
  }

  assert(R->hasDefinition());

  auto PointeeType = getPointeeType(R);
  if (!PointeeType.isNull())
    return PointeeType;

  if (auto *T = dyn_cast<ClassTemplateSpecializationDecl>(R)) {
    auto &Args = T->getTemplateArgs();
    if (Args.size() > 0 && Args[0].getKind() == TemplateArgument::Type)
      return Args[0].getAsType();
  }
  return {};
}

QualType getPointeeType(const Type *T) {
  assert(T);
  T = T->getCanonicalTypeUnqualified().getTypePtr();
  static std::map<const Type *, QualType> M;

  auto I = M.find(T);
  if (I != M.end())
    return I->second;

  // Insert Null before calling getPointeeTypeImpl to stop
  // a possible classifyTypeCategory -> getPointeeType infinite recursion
  M[T] = QualType{};

  auto P = getPointeeTypeImpl(T);
  if (!P.isNull()) {
    P = P.getCanonicalType();
    if (P->isVoidType())
      P = {};
  }
  M[T] = P;
#if CLASSIFY_DEBUG
  llvm::errs() << "DerefType(" << QualType(T, 0).getAsString()
               << ") = " << P.getAsString() << "\n";
#endif
  return P;
}

CallTypes getCallTypes(const Expr *CalleeE) {
  CallTypes CT;

  if (CalleeE->hasPlaceholderType(BuiltinType::BoundMember)) {
    CalleeE = CalleeE->IgnoreParenImpCasts();
    if (const auto *BinOp = dyn_cast<BinaryOperator>(CalleeE)) {
      auto MemberPtr = BinOp->getRHS()->getType()->castAs<MemberPointerType>();
      CT.FTy = MemberPtr->getPointeeType()
                   .IgnoreParens()
                   ->getAs<FunctionProtoType>();
      CT.ClassDecl = MemberPtr->getClass()->getAsCXXRecordDecl();
    } else if (const auto *ME = dyn_cast<MemberExpr>(CalleeE)) {
      CT.FTy = dyn_cast<FunctionProtoType>(ME->getMemberDecl()->getType());
      auto ClassType = ME->getBase()->getType();
      if (ClassType->isPointerType())
        ClassType = ClassType->getPointeeType();
      CT.ClassDecl = ClassType->getAsCXXRecordDecl();
    } else {
      CalleeE->dump();
      llvm_unreachable("not a binOp after boundMember");
    }
    assert(CT.FTy);
    assert(CT.ClassDecl);
    return CT;
  }

  const auto *P =
      dyn_cast<PointerType>(CalleeE->getType()->getUnqualifiedDesugaredType());
  assert(P);
  CT.FTy = dyn_cast<FunctionProtoType>(
      P->getPointeeType()->getUnqualifiedDesugaredType());

  assert(CT.FTy);
  return CT;
}

bool isLifetimeConst(const FunctionDecl *FD, QualType Pointee, int ArgNum) {
  // Until annotations are widespread, STL specific lifetimeconst
  // methods and params can be enumerated here.
  if (!FD)
    return false;

  // std::begin, std::end free functions.
  if (FD->isInStdNamespace() && FD->getDeclName().isIdentifier() &&
      (FD->getName() == "begin" || FD->getName() == "end"))
    return true;

  if (ArgNum >= 0) {
    if (static_cast<size_t>(ArgNum) >= FD->param_size())
      return false;
    auto Param = FD->parameters()[ArgNum];
    return Pointee.isConstQualified() || Param->hasAttr<LifetimeconstAttr>();
  }

  assert(ArgNum == -1);
  if (FD->hasAttr<LifetimeconstAttr>())
    return true;

  if (const auto *MD = dyn_cast<CXXMethodDecl>(FD)) {
    if (MD->isConst())
      return true;
    if (FD->isOverloadedOperator()) {
      return FD->getOverloadedOperator() == OO_Subscript ||
             FD->getOverloadedOperator() == OO_Star ||
             FD->getOverloadedOperator() == OO_Arrow;
    } else {
      return FD->getDeclName().isIdentifier() &&
             (FD->getName() == "at" || FD->getName() == "data" ||
              FD->getName() == "begin" || FD->getName() == "end" ||
              FD->getName() == "rbegin" || FD->getName() == "rend" ||
              FD->getName() == "back" || FD->getName() == "front");
    }
  }
  return false;
}
} // namespace lifetime
} // namespace clang