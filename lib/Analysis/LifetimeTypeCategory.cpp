#include "clang/Analysis/Analyses/LifetimeTypeCategory.h"
#include "clang/AST/DeclCXX.h"

namespace clang {
namespace lifetime {

bool hasMethodWithNameAndArgNum(const CXXRecordDecl *R, StringRef Name,
                                int ArgNum = -1) {
  // TODO cache IdentifierInfo to avoid string compare
  auto CallBack = [Name, ArgNum](const CXXRecordDecl *Base) {
    return std::none_of(Base->method_begin(), Base->method_end(),
                        [Name, ArgNum](const CXXMethodDecl *M) {
                          if (ArgNum >= 0 &&
                              (unsigned)ArgNum != M->getMinRequiredArguments())
                            return false;
                          auto *I = M->getDeclName().getAsIdentifierInfo();
                          if (!I)
                            return false;
                          return I->getName() == Name;
                        });
  };
  return !R->forallBases(CallBack) || !CallBack(R);
}

bool satisfiesContainerRequirements(const CXXRecordDecl *R) {
  // TODO https://en.cppreference.com/w/cpp/named_req/Container
  return hasMethodWithNameAndArgNum(R, "begin", 0) &&
         hasMethodWithNameAndArgNum(R, "end", 0) && !R->hasTrivialDestructor();
}

bool satisfiesIteratorRequirements(const CXXRecordDecl *R) {
  // TODO https://en.cppreference.com/w/cpp/named_req/Iterator
  bool hasDeref = false;
  bool hasPlusPlus = false;
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

bool satisfiesRangeConcept(const CXXRecordDecl *R) {
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
Optional<TypeCategory> classifyStd(const Type *T) {
  NamedDecl *Decl;
  if (const auto *TypeDef = T->getAs<TypedefType>()) {
    if (auto TypeCat = classifyStd(TypeDef->desugar().getTypePtr()))
      return TypeCat;
    Decl = TypeDef->getDecl();
  } else
    Decl = T->getAsCXXRecordDecl();
  auto DeclName = Decl->getDeclName();
  if (!DeclName || !DeclName.isIdentifier())
    return {};

  if (!Decl->isInStdNamespace())
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
    if (T->isArrayType())
      return TypeCategory::Aggregate; // TODO not in the paper

    // raw pointers and references
    if (T->isPointerType() || T->isReferenceType())
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

  // TODO: handle outside class definition 'R& operator*(T a);' and inheritance.
  bool hasDerefOperations = std::any_of(
      R->method_begin(), R->method_end(), [](const CXXMethodDecl *M) {
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

} // namespace lifetime
} // namespace clang