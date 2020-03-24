//=- LifetimeTypeCategory.cpp - Diagnose lifetime violations -*- C++ -*-======//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/Analysis/Analyses/LifetimeTypeCategory.h"
#include "clang/AST/Attr.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"
#include <map>

#define CLASSIFY_DEBUG 0

namespace clang {
namespace lifetime {

static QualType getPointeeType(const Type *T);

template <typename T>
static bool hasMethodLike(const CXXRecordDecl *R, T Predicate,
                          const CXXMethodDecl **FoundMD = nullptr) {
  // TODO cache IdentifierInfo to avoid string compare
  auto CallBack = [Predicate, FoundMD](const CXXRecordDecl *Base) {
    return std::none_of(
        Base->decls_begin(), Base->decls_end(),
        [Predicate, FoundMD](const Decl *D) {
          if (auto *M = dyn_cast<CXXMethodDecl>(D)) {
            bool Found = Predicate(M);
            if (Found && FoundMD)
              *FoundMD = M;
            return Found;
          }
          if (auto *Tmpl = dyn_cast<FunctionTemplateDecl>(D)) {
            if (auto *M = dyn_cast<CXXMethodDecl>(Tmpl->getTemplatedDecl())) {
              bool Found = Predicate(M);
              if (Found && FoundMD)
                *FoundMD = M;
              return Found;
            }
          }
          return false;
        });
  };
  return !R->forallBases(CallBack) || !CallBack(R);
}

static bool hasOperator(const CXXRecordDecl *R, OverloadedOperatorKind Op,
                        int NumParams = -1, bool ConstOnly = false,
                        const CXXMethodDecl **FoundMD = nullptr) {
  return hasMethodLike(
      R,
      [Op, NumParams, ConstOnly](const CXXMethodDecl *MD) {
        if (NumParams != -1 && NumParams != (int)MD->param_size())
          return false;
        if (ConstOnly && !MD->isConst())
          return false;
        return MD->getOverloadedOperator() == Op;
      },
      FoundMD);
}

static bool
hasMethodWithNameAndArgNum(const CXXRecordDecl *R, StringRef Name,
                           int ArgNum = -1,
                           const CXXMethodDecl **FoundMD = nullptr) {
  return hasMethodLike(
      R,
      [Name, ArgNum](const CXXMethodDecl *M) {
        if (ArgNum >= 0 && (unsigned)ArgNum != M->getMinRequiredArguments())
          return false;
        auto *I = M->getDeclName().getAsIdentifierInfo();
        if (!I)
          return false;
        return I->getName() == Name;
      },
      FoundMD);
}

static bool satisfiesContainerRequirements(const CXXRecordDecl *R) {
  // TODO https://en.cppreference.com/w/cpp/named_req/Container
  return hasMethodWithNameAndArgNum(R, "begin", 0) &&
         hasMethodWithNameAndArgNum(R, "end", 0) && !R->hasTrivialDestructor();
}

static bool satisfiesIteratorRequirements(const CXXRecordDecl *R) {
  // TODO https://en.cppreference.com/w/cpp/named_req/Iterator
  // TODO operator* might be defined as a free function.
  return hasOperator(R, OO_Star, 0) && hasOperator(R, OO_PlusPlus, 0);
}

static bool satisfiesRangeConcept(const CXXRecordDecl *R) {
  // TODO https://en.cppreference.com/w/cpp/experimental/ranges/range/Range
  return hasMethodWithNameAndArgNum(R, "begin", 0) &&
         hasMethodWithNameAndArgNum(R, "end", 0) && R->hasTrivialDestructor();
}

static bool hasDerefOperations(const CXXRecordDecl *R) {
  // TODO operator* might be defined as a free function.
  return hasOperator(R, OO_Arrow, 0) || hasOperator(R, OO_Star, 0);
}

/// Determines if D is std::vector<bool>::reference
static bool IsVectorBoolReference(const CXXRecordDecl *D) {
  assert(D);
  static std::set<StringRef> StdVectorBoolReference{
      "__bit_const_reference" /* for libc++ */,
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
  auto *Decl = T->getAsCXXRecordDecl();
  if (!Decl || !Decl->isInStdNamespace() || !Decl->getIdentifier())
    return {};

  if (IsVectorBoolReference(Decl))
    return TypeCategory::Pointer;

  return {};
}

/// Checks if all bases agree to the same TypeClassification,
/// and if they do, returns it.
static Optional<TypeClassification>
getBaseClassification(const CXXRecordDecl *R) {
  QualType PointeeType;
  bool HasOwnerBase = false;
  bool HasPointerBase = false;
  bool PointeesDisagree = false;

  for (const CXXBaseSpecifier &B : R->bases()) {
    auto C = classifyTypeCategory(B.getType());
    if (C.TC == TypeCategory::Owner) {
      HasOwnerBase = true;
    } else if (C.TC == TypeCategory::Pointer) {
      HasPointerBase = true;
    } else {
      continue;
    }
    if (PointeeType.isNull())
      PointeeType = C.PointeeType;
    else if (PointeeType != C.PointeeType) {
      PointeesDisagree = true;
    }
  }

  if (!HasOwnerBase && !HasPointerBase)
    return {};

  if (HasOwnerBase && HasPointerBase)
    return TypeClassification(TypeCategory::Value);

  if (PointeesDisagree)
    return TypeClassification(TypeCategory::Value);

  assert(HasOwnerBase ^ HasPointerBase);
  if (HasPointerBase)
    return TypeClassification(TypeCategory::Pointer, PointeeType);
  else
    return TypeClassification(TypeCategory::Owner, PointeeType);
}

static TypeClassification classifyTypeCategoryImpl(const Type *T) {
  assert(T);
  auto *R = T->getAsCXXRecordDecl();

  if (!R) {
    if (T->isVoidPointerType())
      return TypeCategory::Value;

    if (T->isArrayType())
      return {TypeCategory::Owner, getPointeeType(T)};

    // raw pointers and references
    if (T->isPointerType() || T->isReferenceType())
      return {TypeCategory::Pointer, getPointeeType(T)};

    return TypeCategory::Value;
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
      Pointee = R->getASTContext().VoidTy;
    return {TypeCategory::Owner, Pointee};
  }

  if (R->hasAttr<PointerAttr>()) {
    if (Pointee.isNull())
      Pointee = R->getASTContext().VoidTy;
    return {TypeCategory::Pointer, Pointee};
  }

  // Do not attempt to infer implicit Pointer/Owner if we cannot deduce
  // the DerefType.
  if (!Pointee.isNull()) {

    if (auto Cat = classifyStd(T))
      return {*Cat, Pointee};

    // Every type that satisfies the standard Container requirements.
    if (satisfiesContainerRequirements(R))
      return {TypeCategory::Owner, Pointee};

    // Every type that provides unary * or -> and has a user-provided
    // destructor. (Example: unique_ptr.)
    if (hasDerefOperations(R) && !R->hasTrivialDestructor())
      return {TypeCategory::Owner, Pointee};

    //  Every type that satisfies the Ranges TS Range concept.
    if (satisfiesRangeConcept(R))
      return {TypeCategory::Pointer, Pointee};

    // Every type that satisfies the standard Iterator requirements. (Example:
    // regex_iterator.), see
    // https://en.cppreference.com/w/cpp/named_req/Iterator
    if (satisfiesIteratorRequirements(R))
      return {TypeCategory::Pointer, Pointee};

    // Every type that provides unary * or -> and does not have a user-provided
    // destructor. (Example: span.)
    if (hasDerefOperations(R) && R->hasTrivialDestructor())
      return {TypeCategory::Pointer, Pointee};
  }

  // Every closure type of a lambda that captures by reference.
  if (R->isLambda()) {
    return TypeCategory::Value;
  }

  if (auto C = getBaseClassification(R))
    return *C;

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
  T = T->getUnqualifiedDesugaredType();

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

  for (auto D : R->decls()) {
    if (const auto *TypeDef = dyn_cast<TypedefNameDecl>(D)) {
      if (TypeDef->getName() == "value_type")
        return TypeDef->getUnderlyingType().getCanonicalType();
    }
  }

  // TODO operator* might be defined as a free function.
  struct OpTy {
    OverloadedOperatorKind Kind;
    int ParamNum;
    bool ConstOnly;
  };
  OpTy Ops[] = {
      {OO_Star, 0, false}, {OO_Arrow, 0, false}, {OO_Subscript, -1, true}};
  for (auto P : Ops) {
    const CXXMethodDecl *F;
    if (hasOperator(R, P.Kind, P.ParamNum, P.ConstOnly, &F) &&
        !F->isDependentContext()) {
      auto PointeeType = F->getReturnType();
      if (PointeeType->isReferenceType() || PointeeType->isAnyPointerType())
        PointeeType = PointeeType->getPointeeType();
      if (P.ConstOnly)
        return PointeeType.getCanonicalType().getUnqualifiedType();
      return PointeeType.getCanonicalType();
    }
  }

  const CXXMethodDecl *FoundMD;
  if (hasMethodWithNameAndArgNum(R, "begin", 0, &FoundMD)) {
    auto PointeeType = FoundMD->getReturnType();
    if (classifyTypeCategory(PointeeType) != TypeCategory::Pointer) {
#if CLASSIFY_DEBUG
      // TODO: diag?
      llvm::errs() << "begin() function does not return a Pointer!\n";
      FoundMD->dump();
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
    return R->getASTContext().BoolTy;

  if (!R->hasDefinition())
    return {};

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

/// WARNING: This overload does not consider base classes.
/// Use classifyTypeCategory(T).PointeeType to consider base classes.
static QualType getPointeeType(const Type *T) {
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
    }
    const CXXRecordDecl *RD = MD->getParent();
    StringRef ClassName = RD->getName();
    if (RD->isInStdNamespace()) {
      if (ClassName.endswith("map") || ClassName.endswith("set")) {
        if (FD->getDeclName().isIdentifier() &&
            (FD->getName() == "insert" || FD->getName() == "emplace" ||
             FD->getName() == "emplace_hint"))
          return true;
      }
      if (ClassName == "list" || ClassName == "forward_list")
        return FD->getDeclName().isIdentifier() && FD->getName() != "clear" &&
               FD->getName() != "assign";
    }
    return FD->getDeclName().isIdentifier() &&
           (FD->getName() == "at" || FD->getName() == "data" ||
            FD->getName() == "begin" || FD->getName() == "end" ||
            FD->getName() == "rbegin" || FD->getName() == "rend" ||
            FD->getName() == "back" || FD->getName() == "front");
  }
  return false;
}
} // namespace lifetime
} // namespace clang
