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
  NamedDecl *Decl;
  if (const auto *TypeDef = T->getAs<TypedefType>()) {
    if (auto TypeCat = classifyStd(TypeDef->desugar().getTypePtr()))
      return TypeCat;
    Decl = TypeDef->getDecl();
    if (Decl->getName() == "reference") {
      const auto *RD = dyn_cast<CXXRecordDecl>(Decl->getDeclContext());
      if (satisfiesContainerRequirements(RD) && RD->isInStdNamespace()) {
        // Lazily populate the list of pointers with the name of the
        // implementation defined proxy classes.
        if ((Decl = TypeDef->desugar()->getAsCXXRecordDecl()))
          StdPointers.insert(Decl->getName());
        return TypeCategory::Pointer;
      }
    }
  } else
    Decl = T->getAsCXXRecordDecl();
  if (!Decl || !Decl->isInStdNamespace() || !Decl->getIdentifier())
    return {};

  if (StdOwners.count(Decl->getName()))
    return TypeCategory::Owner;
  if (StdPointers.count(Decl->getName()))
    return TypeCategory::Pointer;

  return {};
}

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

  if (!R->hasDefinition())
    return TypeCategory::Value;

  if (R->hasAttr<OwnerAttr>())
    return TypeCategory::Owner;

  if (R->hasAttr<PointerAttr>())
    return TypeCategory::Pointer;

  // In case we do not know the pointee type fall back to value.
  /*QualType Pointee = getPointeeType(QT);
  if (Pointee.isNull())
    return TypeCategory::Value;*/

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
  if (hasDerefOperations && !R->hasTrivialDestructor())
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
        return classifyTypeCategory(FD->getType()) == TypeCategory::Pointer;
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
  return classifyTypeCategory(QT) == TypeCategory::Pointer &&
         !QT->isReferenceType();
}

QualType getPointeeType(QualType QT) {
  QT = QT.getCanonicalType();
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
      // Heuristic for vector<bool>::reference. Return void, we do not
      // want it to alias with pointers to bool. TODO: revise.
      if (M->getIdentifier() && M->getName() == "flip")
        return M->getReturnType();
    }
    // Check the bases.
    for (auto Base : R->bases()) {
      QualType Ret = getPointeeType(Base.getType());
      if (!Ret.isNull())
        return Ret;
    }

    if (auto *T = dyn_cast<ClassTemplateSpecializationDecl>(R)) {
      auto &Args = T->getTemplateArgs();
      if (Args.size() > 0 && Args[0].getKind() == TemplateArgument::Type)
        return Args[0].getAsType();
    }
  }
  return {};
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