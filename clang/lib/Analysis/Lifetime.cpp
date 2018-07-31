//===--- ProLifetimeVisitor.cpp ---------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
// The pset of a (pointer/reference) variable can be modified by
//   1) Initialization
//   2) Assignment
// It will be set to the pset of the expression on the right-hand-side.
// Such expressions can contain:
//   1) Casts: Ignored, pset(expr) == pset((cast)expr)
//   2) Address-Of operator:
//        It can only be applied to lvalues, i.e.
//        VarDecl, pset(&a) = {a}
//        a function returning a ref,
//        result of an (compound) assignment, pset(&(a = b)) == {b}
//        pre-in/decrement, pset(&(--a)) = {a}
//        deref,
//        array subscript, pset(&a[3]) = {a}
//        a.b, a.*b: pset(&a.b) = {a}
//        a->b, a->*b,
//        comma expr, pset(&(a,b)) = {b}
//        string literal, pset(&"string") = {static}
//        static_cast<int&>(x)
//
//   3) Dereference operator
//   4) Function calls and CXXMemberCallExpr
//   5) MemberExpr: pset(this->a) = {a}; pset_ref(o->a) = {o}; pset_ptr(o->a) =
//   {o'}
//   6) Ternary: pset(cond ? a : b) == pset(a) union pset(b)
//   7) Assignment operator: pset(a = b) == {b}
// Rules:
//  1) T& p1 = expr; T* p2 = &expr; -> pset(p1) == pset(p2) == pset_ref(expr)
//  2) T& p1 = *expr; T* p2 = expr; -> pset(p1) == pset(p2) == pset_ptr(expr)
//  3) Casts are ignored: pset(expr) == pset((cast)expr)
//  4) T* p1 = &C.m; -> pset(p1) == {C} (per ex. 1.3)
//  5) T* p2 = C.get(); -> pset(p2) == {C'} (per ex. 8.1)
//
// Assumptions:
// - The 'this' pointer cannot be invalidated inside a member method (enforced
// here: no delete on *this)
// - Global variable's pset is (static) and/or (null) (enforced here)
// - Arithmetic on pointer types is forbidden (enforced by
// cppcoreguidelines-pro-type-pointer-arithmetic)
// - A function does not modify const arguments (enforced by
// cppcoreguidelines-pro-type-pointer-arithmetic)
// - An access to an array through array subscription always points inside the
// array (enforced by cppcoreguidelines-pro-bounds)
//
// TODO:
//  track psets for objects containing Pointers (e.g. iterators)
//  handle function/method call in PSetFromExpr
//  check correct use of gsl::owner<> (delete only on 'valid' owner, delete must
//  happen before owner goes out of scope)
//  handle try-catch blocks (AC.getCFGBuildOptions().AddEHEdges = true)
//
//===----------------------------------------------------------------------===//

#include "clang/Analysis/Analyses/Lifetime.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/Analysis/Analyses/PostOrderCFGView.h"
#include "clang/Analysis/CFG.h"
#include "clang/Sema/SemaDiagnostic.h" // TODO: remove me and move all diagnostics into LifetimeReporter
#include <algorithm>
#include <map>
#include <sstream>
#include <unordered_map>

namespace clang {
namespace {

bool hasMethodWithName(const CXXRecordDecl *R, StringRef Name) {
  // TODO CXXRecordDecl::forallBases
  // TODO cache IdentifierInfo to avoid string compare
  return std::any_of(R->method_begin(), R->method_end(),
                     [Name](const CXXMethodDecl *M) {
                       auto *I = M->getDeclName().getAsIdentifierInfo();
                       if (!I)
                         return false;
                       return I->getName() == Name;
                     });
}

bool satisfiesContainerRequirements(const CXXRecordDecl *R) {
  // TODO https://en.cppreference.com/w/cpp/named_req/Container
  return hasMethodWithName(R, "begin") && hasMethodWithName(R, "end") &&
         R->hasUserDeclaredDestructor();
}

bool satisfiesIteratorRequirements(const CXXRecordDecl *R) {
  // TODO https://en.cppreference.com/w/cpp/named_req/Iterator
  bool hasDeref = false;
  bool hasPlusPlus = false;
  for(auto* M : R->methods()) {
	 auto O = M->getDeclName().getCXXOverloadedOperator();
	 if(O == OO_PlusPlus)
		 hasPlusPlus = true;
	 else if(O == OO_Star && M->param_empty())
		 hasDeref = true;
	 if(hasPlusPlus && hasDeref)
		 return true;
  }
  return false;
}

bool satisfiesRangeConcept(const CXXRecordDecl *R) {
  // TODO https://en.cppreference.com/w/cpp/experimental/ranges/range/Range
  return hasMethodWithName(R, "begin") && hasMethodWithName(R, "end") &&
         !R->hasUserDeclaredDestructor();
}

/// classifies some well-known std:: types or returns an empty optional
Optional<TypeCategory> classifyStd(const CXXRecordDecl *R) {
  auto DeclName = R->getDeclName();
  if (!DeclName || !DeclName.isIdentifier())
    return {};

  if (!R->isInStdNamespace())
    return {};

  // FIXME faster lookup (e.g. (sorted) vector)
  static std::set<StringRef> StdOwners{"stack",    "queue",   "priority_queue",
                                       "optional", "variant", "any"};
  static std::set<StringRef> StdPointers{"basic_regex", "reference_wrapper",
                                         "vector<bool>::reference"};

  if (StdOwners.count(DeclName.getAsIdentifierInfo()->getName()))
    return TypeCategory::Owner;
  if (StdPointers.count(DeclName.getAsIdentifierInfo()->getName()))
    return TypeCategory::Pointer;

  return {};
}

/// Returns the type category of the given type
/// If T is a template specialization, it must be instantiated.
TypeCategory classifyTypeCategory(const Type *T) {
  /*
          llvm::errs() << "classifyTypeCategory\n ";
           T->dump(llvm::errs());
           llvm::errs() << "\n";*/

  const auto *R = T->getAsCXXRecordDecl();

  if (!R) {
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

  //	* Every type that satisfies the standard Container requirements.
  if (satisfiesContainerRequirements(R))
    return TypeCategory::Owner;

  // TODO: handle outside class definition 'R& operator*(T a);'
  bool hasDerefOperations = std::any_of(
      R->method_begin(), R->method_end(), [](const CXXMethodDecl *M) {
        auto O = M->getDeclName().getCXXOverloadedOperator();
        return (O == OO_Arrow) || (O == OO_Star && M->param_empty());
      });

  // Every type that provides unary * or -> and has a user-provided destructor.
  // (Example: unique_ptr.)
  if (hasDerefOperations && R->hasUserDeclaredDestructor())
    return TypeCategory::Owner;

  if (auto Cat = classifyStd(R))
    return *Cat;

  //  Every type that satisfies the Ranges TS Range concept.
  if (satisfiesRangeConcept(R))
    return TypeCategory::Pointer;

  // * Every type that satisfies the standard Iterator requirements. (Example:
  // regex_iterator.), see https://en.cppreference.com/w/cpp/named_req/Iterator
  if (satisfiesIteratorRequirements(R))
    return TypeCategory::Pointer;

  // * Every type that provides unary * or -> and does not have a user-provided
  // destructor. (Example: span.)
  if (hasDerefOperations && !R->hasUserDeclaredDestructor())
    return TypeCategory::Pointer;

  // * Every closure type of a lambda that captures by reference.
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

/// A Pointer is a
/// 1) VarDecl or
/// 2) FieldDecl (on 'this')
/// and
/// a) with pointer type, e.g. p in 'int* p' or
/// b) with reference type, e.g. p in 'int& p' or
/// c) of an object that contains a Pointer, e.g. p in 'struct { int* q; } p;'
/// or
/// TODO: implement case b) and c)
/// Invariant: VD != FD && (!VD || !FD)
class Pointer {
  const VarDecl *VD = nullptr;
  const FieldDecl *FD = nullptr;

public:
  Pointer(const VarDecl *VD) : VD(VD) { assert(VD); }
  Pointer(const FieldDecl *FD) : FD(FD) { assert(FD); }
  Pointer(Pointer &&) = default;
  Pointer &operator=(Pointer &&) = default;
  Pointer(const Pointer &) = default;
  Pointer &operator=(const Pointer &) = default;

  bool operator==(const Pointer &o) const { return VD == o.VD && FD == o.FD; }
  bool operator!=(const Pointer &o) const { return !(*this == o); }
  bool operator<(const Pointer &o) const {
    if (VD != o.VD)
      return VD < o.VD;
    return FD < o.FD;
  }

  QualType getCanonicalType() const {
    if (VD)
      return VD->getType().getCanonicalType();
    return FD->getType().getCanonicalType();
  }

  StringRef getName() const {
    if (VD)
      return VD->getName();
    return FD->getName();
  }

  bool hasGlobalStorage() const { return VD && VD->hasGlobalStorage(); }
  /// Returns true if this pointer is a member variable of the class of the
  /// current method
  bool isMemberVariable() const { return FD; }

  bool mayBeNull() const {
    // TODO: check if the type is gsl::not_null
    return isa<PointerType>(getCanonicalType());
  }

  bool isRealPointer() const { return getCanonicalType()->isPointerType(); }
  bool isReference() const { return getCanonicalType()->isReferenceType(); }

  std::size_t hash() const noexcept {
    return VD ? std::hash<const VarDecl *>()(VD)
              : std::hash<const FieldDecl *>()(FD);
  }

  // Returns either a Pointer or None if ValD is not a Pointer
  static Optional<Pointer> get(const ValueDecl *ValD) {
    if (!ValD)
      return Optional<Pointer>();

    if (!Pointer::is(ValD->getType().getCanonicalType()))
      return Optional<Pointer>();

    if (auto *VD = dyn_cast<VarDecl>(ValD))
      return {VD};
    if (auto *FD = dyn_cast<FieldDecl>(ValD))
      return {FD};

    return Optional<Pointer>(); // TODO: can this happen?
  }

  // static bool is(const ValueDecl *VD) { return get(VD).hasValue(); }

  /// Returns true if the given type has a pset
  static bool is(QualType QT) {
    return QT->isReferenceType() || QT->isPointerType();
  }
};
} // namespace
} // namespace clang

namespace std {
template <> struct hash<clang::Pointer> {
  std::size_t operator()(const clang::Pointer &P) const noexcept {
    return P.hash();
  }
};
} // namespace std

namespace clang {
namespace {
/// An owner is anything that a Pointer can point to
/// Invariant: VD != nullptr
class Owner {
  enum SpecialType { VARDECL = 0, NULLPTR = 1, STATIC = 2, TEMPORARY = 3 };

  llvm::PointerIntPair<const ValueDecl *, 2> VD;

  Owner(SpecialType ST) : VD(nullptr, ST) {}

public:
  Owner(const ValueDecl *VD) : VD(VD, VARDECL) {
    assert(VD);
    if (const auto *VarD = dyn_cast<VarDecl>(VD))
      if (VarD->hasGlobalStorage())
        *this = Static();
  }
  bool operator==(const Owner &O) const { return VD == O.VD; }
  bool operator!=(const Owner &O) const { return !(*this == O); }
  bool operator<(const Owner &O) const { return VD < O.VD; }

  std::string getName() const {
    switch (VD.getInt()) {
    case VARDECL:
      return VD.getPointer()->getNameAsString();
    case NULLPTR:
      return "(null)";
    case STATIC:
      return "(static)";
    case TEMPORARY:
      return "(temporary)";
    }
    llvm_unreachable("Unexpected type");
  }

  /// Special Owner to refer to a nullptr
  static Owner Null() { return Owner(NULLPTR); }

  /// An owner that can not be invalidated.
  /// Used for variables with static duration
  static Owner Static() { return Owner(STATIC); }

  /// An owner that will vanish at the end of the full expression,
  /// e.g. a temporary bound to const reference parameter.
  static Owner Temporary() { return Owner(TEMPORARY); }

  /// Returns either a Pointer or None if Owner is not a Pointer
  Optional<Pointer> getPointer() const {
    if (VD.getInt() == VARDECL)
      return Pointer::get(VD.getPointer());
    return Optional<Pointer>();
  }
};

/// The reason why a pset became invalid
/// Invariant: (Reason != POINTEE_LEFT_SCOPE || Pointee) && Loc.isValid()
class InvalidationReason {
  enum { NOT_INITIALIZED, POINTEE_LEFT_SCOPE, TEMPORARY_LEFT_SCOPE } Reason;
  const VarDecl *Pointee = nullptr;
  SourceLocation Loc;

public:
  SourceLocation getLoc() const { return Loc; }

  void emitNotes(const LifetimeReporterBase &Reporter) const {
    switch (Reason) {
    case NOT_INITIALIZED:
      Reporter.diag(Loc, diag::note_never_initialized);
      return;
    case POINTEE_LEFT_SCOPE:
      assert(Pointee);
      Reporter.notePointeeLeftScope(Loc, Pointee->getNameAsString());
      return;
    case TEMPORARY_LEFT_SCOPE:
      Reporter.diag(Loc, diag::note_temporary_destroyed);
      return;
    }
    llvm_unreachable("Invalid InvalidationReason::Reason");
  }

  static InvalidationReason NotInitialized(SourceLocation Loc) {
    assert(Loc.isValid());
    InvalidationReason R;
    R.Loc = Loc;
    R.Reason = NOT_INITIALIZED;
    return R;
  }

  static InvalidationReason PointeeLeftScope(SourceLocation Loc,
                                             const VarDecl *Pointee) {
    assert(Loc.isValid());
    assert(Pointee);
    InvalidationReason R;
    R.Loc = Loc;
    R.Reason = POINTEE_LEFT_SCOPE;
    R.Pointee = Pointee;
    return R;
  }

  static InvalidationReason TemporaryLeftScope(SourceLocation Loc) {
    assert(Loc.isValid());
    InvalidationReason R;
    R.Loc = Loc;
    R.Reason = TEMPORARY_LEFT_SCOPE;
    return R;
  }
};

/// A pset (points-to set) can be unknown, invalid or valid.
/// If it is valid, it can contain (static), (null) and a set of (Owner,order)
/// Invariant: (!isInvalid() || Reason.hasValue())
///         && (!isValid() || p.size() > 0)
class PSet {
public:
  enum State {
    Valid,           // the pointer points to one element of p
    Unknown,         // we lost track of what it could point to
    PossiblyInvalid, // the pointer had a pset containing multiple objects, and
                     // one of them was killed
    Invalid, // the pointer was never initialized or the single member of its
             // pset was killed
  };

  PSet() : state(Unknown) {}

  PSet(PSet &&) = default;
  PSet &operator=(PSet &&) = default;
  PSet(const PSet &) = default;
  PSet &operator=(const PSet &) = default;

  bool operator==(const PSet &o) const {
    if (state == Valid)
      return o.p == p;
    return state == o.state;
  }

  bool operator!=(const PSet &o) const { return !(*this == o); }

  State getState() const { return state; }

  InvalidationReason getReason() const {
    assert(isInvalid());
    assert(Reason.hasValue());
    return Reason.getValue();
  }

  bool isValid() const { return state == Valid; }

  bool isInvalid() const {
    return state == Invalid || state == PossiblyInvalid;
  }

  bool isUnknown() const { return state == Unknown; }

  /// Returns true if this pset contains Owner with the same or a lower order
  /// i.e. if invalidating (Owner, order) would invalidate this pset
  bool contains(Owner Owner, unsigned order) const {
    if (state != Valid)
      return false;

    for (const auto &i : p)
      if (i.first == Owner && i.second >= order)
        return true;

    return false;
  }

  bool isSingular() const { return p.size() == 1; }

  bool containsNull() const { return contains(Owner::Null(), 0); }

  void removeNull() {
    assert(isValid());
    p.erase(Owner::Null());
  }

  bool isSubsetOf(const PSet &PS) {
    assert(getState() != Unknown);
    if (isInvalid())
      return false;

    return std::includes(PS.p.begin(), PS.p.end(), p.begin(), p.end());
  }

  std::string str() const {
    std::stringstream ss;
    switch (state) {
    case Unknown:
      ss << "(unknown)";
      break;
    case Invalid:
      ss << "(invalid)";
      break;
    case PossiblyInvalid:
      ss << "(possibly invalid)";
      break;
    case Valid:
      bool first = true;
      for (const auto &i : p) {
        if (!first)
          ss << ", ";
        else
          first = false;
        ss << i.first.getName();
        for (size_t j = 0; j < i.second; ++j)
          ss << "'";
      }
      break;
    }
    return ss.str();
  }

  void print(raw_ostream &Out) const { Out << str() << "\n"; }

  /// Merge contents of other pset into this
  void merge(const PSet &otherPset) {
    if (state == Unknown || otherPset.state == Unknown) {
      state = Unknown;
      return;
    }
    if (state == Invalid)
      return;
    if (otherPset.state == Invalid) {
      state = Invalid;
      assert(otherPset.Reason.hasValue());
      Reason = otherPset.Reason;
      return;
    }
    if (state == PossiblyInvalid)
      return;
    if (otherPset.state == PossiblyInvalid) {
      state = PossiblyInvalid;
      assert(otherPset.Reason.hasValue());
      Reason = otherPset.Reason;
      return;
    }
    assert(state == Valid && otherPset.state == Valid);

    for (const auto &PO : otherPset.p) {
      auto P = p.find(PO.first);
      if (P == p.end()) {
        p.insert(PO);
      } else {
        // if p contains obj' and otherPset.p contains obj''
        // then the union shall be invalidated whenever obj' or obj'' is
        // invalidated
        // which is the same as whenever obj'' is invalidated
        P->second = std::max(P->second, PO.second);
      }
    }
  }

  /// The pointer is dangling
  static PSet invalid(InvalidationReason Reason) {
    PSet ret;
    ret.state = Invalid;
    ret.Reason = Reason;
    return ret;
  }

  /// We don't know the state of the pointer
  static PSet unknown() {
    PSet ret;
    ret.state = Unknown;
    return ret;
  }

  /// The pset contains nothing
  static PSet valid() {
    PSet ret;
    ret.state = Valid;
    return ret;
  }

  /// The pset contains one of obj, obj' or obj''
  static PSet validOwner(Owner Owner, unsigned order = 0) {
    PSet ret;
    ret.state = Valid;
    ret.p.emplace(Owner, order);
    return ret;
  }

  /// The pset contains null and one of obj, obj' or obj''
  static PSet validOwnerOrNull(Owner Owner, unsigned order = 0) {
    PSet ret = validOwner(Owner, order);
    ret.p.emplace(Owner::Null(), 0);
    return ret;
  }

  std::map<Owner, unsigned>::const_iterator begin() const { return p.begin(); }

  std::map<Owner, unsigned>::const_iterator end() const { return p.end(); }

  void insert(Owner Owner, unsigned order = 0) {
    p.insert(std::make_pair(Owner, order));
  }

private:
  State state = Unknown;
  /// Maps owner obj to order.
  /// (obj,0) == obj: points to obj
  /// (obj,1) == obj': points to object owned directly by obj
  /// (obj,2) == obj'': points an object kept alive indirectly (transitively)
  /// via owner obj
  std::map<Owner, unsigned> p;

  Optional<InvalidationReason> Reason;
};

using PSetsMap = std::unordered_map<Pointer, PSet>;

// Collection of methods to update/check PSets from statements/expressions
class PSetsBuilder {

  const LifetimeReporterBase *Reporter;
  ASTContext &ASTCtxt;
  PSetsMap &PSets;

  /// IgnoreParenImpCasts - Ignore parentheses and implicit casts.  Strip off
  /// any ParenExpr
  /// or ImplicitCastExprs, returning their operand.
  /// Does not ignore MaterializeTemporaryExpr as Expr::IgnoreParenImpCasts
  /// would.
  static const Expr *IgnoreParenImpCasts(const Expr *E) {
    while (true) {
      E = E->IgnoreParens();
      if (const auto *P = dyn_cast<ImplicitCastExpr>(E)) {
        E = P->getSubExpr();
        continue;
      }
      return E;
    }
  }

  // Evaluate the given expression for all effects on psets of variables
  void EvalExpr(const Expr *E) {
    E = E->IgnoreParenCasts();

    switch (E->getStmtClass()) {
    case Expr::BinaryOperatorClass: {
      const auto *BinOp = cast<BinaryOperator>(E);
      Optional<Pointer> P = PointerFromExpr(BinOp->getLHS());
      if (P.hasValue() && P->isRealPointer()) {
        switch (BinOp->getOpcode()) {
        case BO_Assign: {
          // This assignment updates a Pointer
          EvalExpr(BinOp->getLHS()); // Eval for side-effects
          PSet PS = EvalExprForPSet(BinOp->getRHS(), false);
          SetPSet(P.getValue(), PS, BinOp->getExprLoc());
          return;
        }
        case BO_AddAssign:
        case BO_SubAssign:
          // Affects pset; forbidden by the bounds profile.
          SetPSet(P.getValue(), PSet::unknown(), BinOp->getExprLoc());
        default:
          break; // Traversing is done below
        }
      }
      break;
    }
    case Expr::UnaryOperatorClass: {
      const auto *UnOp = cast<UnaryOperator>(E);
      Optional<Pointer> P = PointerFromExpr(UnOp->getSubExpr());
      if (P.hasValue() && P->isRealPointer()) {
        switch (UnOp->getOpcode()) {
        case UO_Deref: {
          // Check if dereferencing this pointer is valid
          PSet PS = EvalExprForPSet(UnOp->getSubExpr(), false);
          CheckPSetValidity(PS, UnOp->getExprLoc());
          return;
        }
        case UO_PostInc:
        case UO_PostDec:
        case UO_PreInc:
        case UO_PreDec:
          // Affects pset; forbidden by the bounds profile.
          SetPSet(P.getValue(), PSet::unknown(), UnOp->getExprLoc());
        default:
          break; // Traversing is done below
        }
      }
      break;
    }
    case Expr::MemberExprClass: {
      const auto *MemberE = cast<MemberExpr>(E);
      const Expr *Base = MemberE->getBase();
      // 'this' can never dangle
      // TODO: check pset for !isArrow if Base is a reference
      if (!isa<CXXThisExpr>(Base) && MemberE->isArrow()) {
        PSet PS = EvalExprForPSet(Base, false);
        CheckPSetValidity(PS, MemberE->getExprLoc());
        return;
      }
      break;
    }
    case Expr::CallExprClass: {
      if (EvalCallExpr(cast<CallExpr>(E)))
        return;
      break;
    }
    default:;
    }
    // Other Expr, just recurse
    for (const Stmt *SubStmt : E->children())
      EvalStmt(SubStmt);
  }

  /// Evaluates the CallExpr for effects on psets.
  /// When a non-const pointer to pointer or reference to pointer is passed
  /// into a function, it's pointee's are invalidated.
  /// Returns true if CallExpr was handled.
  bool EvalCallExpr(const CallExpr *CallE) {
    EvalExpr(CallE->getCallee());

    auto* P = dyn_cast<PointerType>(CallE->getCallee()->getType()->getUnqualifiedDesugaredType());
    assert(P);
    auto* F = dyn_cast<FunctionProtoType>(P->getPointeeType()->getUnqualifiedDesugaredType());
    //F->dump();
    assert(F);
    for (unsigned i = 0; i < CallE->getNumArgs(); ++i) {
      const Expr *Arg = CallE->getArg(i);
      QualType ParamQType = F->isVariadic() ? Arg->getType() : F->getParamType(i);
      const Type* ParamType = ParamQType->getUnqualifiedDesugaredType();

      // TODO implement strong owner rvalue magnets
      // TODO implement gsl::lifetime annotations
      // TODO implement aggregates

      if(auto* R = dyn_cast<ReferenceType>(ParamType)) {
        if(classifyTypeCategory(R->getPointeeType().getTypePtr()) == TypeCategory::Owner) {
          if(isa<RValueReferenceType>(R)) {
            // parameter is in Oin_strong
          } else if(ParamQType.isConstQualified()) {
            // parameter is in Oin_weak
          } else {
            // parameter is in Oin
          }
        } else {
          // parameter is in Pin
        }

      } else if(classifyTypeCategory(ParamType) == TypeCategory::Pointer) {
         // parameter is in Pin
      }

      auto Pointee = ParamType->getPointeeType();
      if(!Pointee.isNull()) {
        if(!Pointee.isConstQualified() &&  classifyTypeCategory(Pointee.getTypePtr()) == TypeCategory::Pointer) {
          // Pout
        }
      }

      // Enforce that psets of all arguments in Oin and Pin do not alias
      // Enforce that pset() of each argument does not refer to a non-const global Owner
      // Enforce that pset() of each argument does not refer to a local Owner in Oin

      PSet PS = EvalExprForPSet(Arg, !ParamType->isPointerType());
      CheckPSetValidity(PS, Arg->getLocStart(), /*flagNull=*/false);
    }
    return true;
  }
  /// Evaluates E for effects that change psets.
  /// 1) if referenceCtx is true, returns the pset of 'p' in 'auto &p =
  /// E';
  /// 2) if referenceCtx is false, returns the pset of 'p' in 'auto *p =
  /// E';
  ///
  /// We use pset_ptr(E) to denote ExprPset when referenceCtx is true
  /// and pset_ref(E) to denote ExprPset when referenceTxt is false.
  /// We use pset(v) when v is a VarDecl to refer to the entry of v in the
  /// PSets
  /// map.
  PSet EvalExprForPSet(const Expr *E, bool referenceCtx) {
    E = IgnoreParenImpCasts(E);

    switch (E->getStmtClass()) {
    case Expr::ExprWithCleanupsClass: {
      const auto *EC = cast<ExprWithCleanups>(E);
      EvalExpr(EC->getSubExpr());
      return EvalExprForPSet(EC->getSubExpr(), referenceCtx);
    }
    case Expr::MaterializeTemporaryExprClass: {
      const auto *MaterializeTemporaryE = cast<MaterializeTemporaryExpr>(E);
      assert(referenceCtx);
      EvalExpr(MaterializeTemporaryE->GetTemporaryExpr());

      if (MaterializeTemporaryE->getExtendingDecl())
        return PSet::validOwner(MaterializeTemporaryE->getExtendingDecl(), 1);
      else
        return PSet::validOwner(Owner::Temporary(), 0);
      break;
    }
    case Expr::DeclRefExprClass: {
      const auto *DeclRef = cast<DeclRefExpr>(E);
      const auto *VD = dyn_cast<VarDecl>(DeclRef->getDecl());
      if (VD) {
        if (referenceCtx) {
          // T i; T &j = i; auto &p = j; -> pset_ref(p) = {i}
          if (VD->getType()->isReferenceType())
            return GetPSet(VD);
          else
            // T i; auto &p = i; -> pset_ref(p) = {i}
            return PSet::validOwner(VD, 0);
        } else {
          if (VD->getType()->isArrayType())
            // Forbidden by bounds profile
            // Alternative would be: T a[]; auto *p = a; -> pset_ptr(p) = {a}
            return PSet::unknown();
          else
            // T i; auto *p = i; -> pset_ptr(p) = pset(i)
            return GetPSet(VD);
        }
      }
      break;
    }
    case Expr::ArraySubscriptExprClass: {
      const auto *ArraySubscriptE = cast<ArraySubscriptExpr>(E);
      EvalExpr(ArraySubscriptE->getBase());
      EvalExpr(ArraySubscriptE->getIdx());

      // By the bounds profile, ArraySubscriptExpr is only allowed on arrays
      // (not on pointers),
      // thus the base needs to be a DeclRefExpr.
      const auto *DeclRef = dyn_cast<DeclRefExpr>(
          ArraySubscriptE->getBase()->IgnoreParenImpCasts());
      if (DeclRef) {
        const VarDecl *VD = dyn_cast<VarDecl>(DeclRef->getDecl());
        if (VD && VD->getType().getCanonicalType()->isArrayType() &&
            referenceCtx)
          // T a[3]; -> pset_ref(a[i]) = {a}
          return PSet::validOwner(VD, 0);
      }
      break;
    }
    case Expr::ConditionalOperatorClass: {
      const auto *CO = cast<ConditionalOperator>(E);
      // pset_ref(b ? a : b) = pset_ref(a) union pset_ref(b)
      // pset_ptr(b ? a : b) = pset_ptr(a) union pset_ptr(b)
      EvalExpr(CO->getCond());

      PSet PSLHS = EvalExprForPSet(CO->getLHS(), referenceCtx);
      PSet PSRHS = EvalExprForPSet(CO->getRHS(), referenceCtx);
      PSRHS.merge(PSLHS);
      return PSRHS;
    }
    case Expr::MemberExprClass: {
      const auto *MemberE = cast<MemberExpr>(E);
      const Expr *Base = MemberE->getBase();
      if (isa<CXXThisExpr>(Base)) {
        // We are inside the class, so track the members separately
        const auto *FD = dyn_cast<FieldDecl>(MemberE->getMemberDecl());
        if (FD) {
          return PSet::validOwner(FD, 0);
        }
      } else {
        return EvalExprForPSet(
            Base, !Base->getType().getCanonicalType()->isPointerType());
      }
      break;
    }
    case Expr::BinaryOperatorClass: {
      const auto *BinOp = cast<BinaryOperator>(E);
      if (BinOp->getOpcode() == BO_Assign) {
        EvalExpr(BinOp);
        return EvalExprForPSet(BinOp->getLHS(), referenceCtx);
      }
      break;
    }
    case Expr::UnaryOperatorClass: {
      const auto *UnOp = cast<UnaryOperator>(E);
      if (UnOp->getOpcode() == UO_Deref) {
        PSet PS = EvalExprForPSet(UnOp->getSubExpr(), false);
        CheckPSetValidity(PS, UnOp->getOperatorLoc());
        if (referenceCtx) {
          // pset_ref(*p) = pset_ptr(p)
          return PS;
        } else {
          if (PS.isInvalid() || PS.isUnknown())
            return PS;

          assert(PS.isValid());
          // pset_ptr(*p) = replace each pset entry of pset_ptr(p) by its own
          // pset
          PSet RetPS = PSet::valid();
          for (auto &Entry : PS) {
            if (Entry.first == Owner::Null())
              continue; // will be flagged by checkPSetValidity above
            if (Entry.first == Owner::Static())
              RetPS.merge(PSet::validOwner(Owner::Static()));
            else {
              Optional<Pointer> P = Entry.first.getPointer();
              if (!P.hasValue())
                // This can happen if P has array type; dereferencing an array
                // is forbidden by the bounds profile
                return PSet::unknown();
              RetPS.merge(GetPSet(P.getValue()));
            }
          }
          return RetPS;
        }
      } else if (UnOp->getOpcode() == UO_AddrOf) {
        assert(!referenceCtx);
        return EvalExprForPSet(UnOp->getSubExpr(), true);
      }
      break;
    }
    case Expr::CXXReinterpretCastExprClass:
    case Expr::CStyleCastExprClass: {
      const auto *CastE = cast<CastExpr>(E);
      switch (CastE->getCastKind()) {
      case CK_BitCast:
      case CK_LValueBitCast:
      case CK_IntegralToPointer:
        // Those casts are forbidden by the type profile
        return PSet::unknown();
      default:
        return EvalExprForPSet(CastE->getSubExpr(), referenceCtx);
      }
    } break;
    default:;
    }

    if (E->isNullPointerConstant(ASTCtxt, Expr::NPC_ValueDependentIsNotNull)) {
      if (referenceCtx) {
        // It is illegal to bind a reference to null
        return PSet::invalid(
            InvalidationReason::NotInitialized(E->getExprLoc()));
      } else {
        return PSet::validOwner(Owner::Null());
      }
    }

    // Unhandled case
    EvalExpr(E);
    return PSet::unknown();
  }

  /// Returns a Pointer if E is a DeclRefExpr of a Pointer
  Optional<Pointer> PointerFromExpr(const Expr *E) {
    E = E->IgnoreParenCasts();
    const auto *DeclRef = dyn_cast<DeclRefExpr>(E);
    if (!DeclRef)
      return Optional<Pointer>();

    return Pointer::get(DeclRef->getDecl());
  }

  void CheckPSetValidity(const PSet &PS, SourceLocation Loc, bool flagNull = true);

  // Traverse S in depth-first post-order and evaluate all effects on psets
  void EvalStmt(const Stmt *S) {
    if (const auto *DS = dyn_cast<DeclStmt>(S)) {
      assert(DS->isSingleDecl());
      const Decl *D = DS->getSingleDecl();
      if (const auto *VD = dyn_cast<VarDecl>(D))
        EvalVarDecl(VD);
    } else if (const auto *E = dyn_cast<Expr>(S))
      EvalExpr(E);
  }

  /// Invalidates all psets that point to V or something owned by V
  void invalidateOwner(Owner O, unsigned order, InvalidationReason Reason) {
    for (auto &i : PSets) {
      const auto &Pointer = i.first;
      PSet &PS = i.second;
      if (!PS.isValid())
        continue; // Nothing to invalidate

      if (PS.contains(O, order))
        SetPSet(Pointer, PSet::invalid(Reason), Reason.getLoc());
    }
  }
  void erasePointer(Pointer P) { PSets.erase(P); }
  PSet GetPSet(Pointer P);
  void SetPSet(Pointer P, PSet PS, SourceLocation Loc);
  void diagPSet(Pointer P, SourceLocation Loc) {
    auto i = PSets.find(P);
    if (i != PSets.end())
      Reporter->debugPset(Loc, P.getName(), i->second.str());
    else
      Reporter->debugPset(Loc, P.getName(), "(untracked)");
  }

  bool HandleClangAnalyzerPset(const Stmt *S,
                               const LifetimeReporterBase *Reporter);

public:
  PSetsBuilder(const LifetimeReporterBase *Reporter, ASTContext &ASTCtxt,
               PSetsMap &PSets)
      : Reporter(Reporter), ASTCtxt(ASTCtxt), PSets(PSets) {}

  void EvalVarDecl(const VarDecl *VD) {
    const Expr *Initializer = VD->getInit();
    auto P = Pointer::get(VD);

    if (!P.hasValue()) {
      // Not a Pointer, but initializer can have side-effects
      if (Initializer)
        EvalExpr(Initializer);
      return;
    }

    SourceLocation Loc = VD->getLocEnd();

    PSet PS; //== unknown

    if (Initializer)
      PS = EvalExprForPSet(Initializer, !P->isRealPointer());
    else if (Loc.isValid())
      PS = PSet::invalid(InvalidationReason::NotInitialized(Loc));

    // We don't track the PSets of variables with global storage; just make
    // sure that its pset is always {static} and/or {null}
    if (P->hasGlobalStorage()) {
      if (PS.isUnknown()) {
        // TODO diagnose?
        return;
      }

      if (!PS.isSubsetOf(PSet::validOwnerOrNull(Owner::Static())) && Reporter)
        Reporter->warnPsetOfGlobal(Loc, P->getName(), PS.str());
      return;
    }

    SetPSet(VD, PS, Loc);
    return;
  }

  void VisitBlock(const CFGBlock &B, const LifetimeReporterBase *Reporter);

  void UpdatePSetsFromCondition(const Expr *E, bool positive,
                                SourceLocation Loc);
};

// Manages lifetime information for the CFG of a FunctionDecl

PSet PSetsBuilder::GetPSet(Pointer P) {
  auto i = PSets.find(P);
  if (i != PSets.end())
    return i->second;

  // Assumption: global Pointers have a pset of {static, null}
  if (P.hasGlobalStorage() || P.isMemberVariable()) {
    if (P.mayBeNull())
      return PSet::validOwnerOrNull(Owner::Static());
    return PSet::validOwner(Owner::Static());
  }

  llvm_unreachable("Missing pset for Pointer");
}

void PSetsBuilder::SetPSet(Pointer P, PSet PS, SourceLocation Loc) {
  assert(P.getName() != "");

  // Assumption: global Pointers have a pset that is a subset of {static,
  // null}
  if (P.hasGlobalStorage() && !PS.isUnknown() &&
      !PS.isSubsetOf(PSet::validOwnerOrNull(Owner::Static())) && Reporter)
    Reporter->warnPsetOfGlobal(Loc, P.getName(), PS.str());

  auto i = PSets.find(P);
  if (i != PSets.end())
    i->second = std::move(PS);
  else
    PSets.emplace(P, PS);
}

void PSetsBuilder::CheckPSetValidity(const PSet &PS, SourceLocation Loc, bool flagNull) {
  if (!Reporter)
    return;

  if (PS.getState() == PSet::Unknown) {
    Reporter->diag(Loc, diag::warn_deref_unknown);
    return;
  }

  if (PS.getState() == PSet::Invalid ||
      PS.getState() == PSet::PossiblyInvalid) {
    Reporter->warnDerefDangling(Loc, PS.getState() == PSet::PossiblyInvalid);
    PS.getReason().emitNotes(*Reporter);
    return;
  }

  if (PS.isValid() && PS.containsNull()) {
    Reporter->warnDerefNull(Loc, !PS.isSingular());
    return;
  }
}

/// Updates psets to remove 'null' when entering conditional statements. If
/// 'positive' is
/// false,
/// handles expression as-if it was negated.
/// Examples:
///   int* p = f();
/// if(p)
///  ... // pset of p does not contain 'null'
/// else
///  ... // pset of p still contains 'null'
/// if(!p)
///  ... // pset of p still contains 'null'
/// else
///  ... // pset of p does not contain 'null'
/// TODO: Add condition like 'p == nullptr', 'p != nullptr', '!(p ==
/// nullptr)',
/// etc
void PSetsBuilder::UpdatePSetsFromCondition(const Expr *E, bool positive,
                                            SourceLocation Loc) {
  E = E->IgnoreParenImpCasts();

  if (positive) {
    if (auto *BO = dyn_cast<BinaryOperator>(E)) {
      if (BO->getOpcode() != BO_LAnd)
        return;
      UpdatePSetsFromCondition(BO->getLHS(), true, Loc);
      UpdatePSetsFromCondition(BO->getRHS(), true, Loc);
      return;
    }

    if (auto *DeclRef = dyn_cast<DeclRefExpr>(E)) {
      Optional<Pointer> P = Pointer::get(DeclRef->getDecl());
      if (!P.hasValue())
        return;
      auto PS = GetPSet(P.getValue());
      if (!PS.isValid())
        return;
      if (PS.containsNull()) {
        PS.removeNull();
        SetPSet(P.getValue(), PS, Loc);
      }
    }
  } else {
    if (auto *BO = dyn_cast<BinaryOperator>(E)) {
      if (BO->getOpcode() != BO_LOr)
        return;
      UpdatePSetsFromCondition(BO->getLHS(), false, Loc);
      UpdatePSetsFromCondition(BO->getRHS(), false, Loc);
      return;
    }

    if (auto *UO = dyn_cast<UnaryOperator>(E)) {
      if (UO->getOpcode() != UO_LNot)
        return;
      UpdatePSetsFromCondition(UO->getSubExpr(), true, Loc);
    }
  }
}

/// Checks if the statement S is a call to clang_analyzer_pset and, if yes,
/// diags the pset of its argument
bool PSetsBuilder::HandleClangAnalyzerPset(
    const Stmt *S, const LifetimeReporterBase *Reporter) {
  assert(Reporter);
  const auto *CallE = dyn_cast<CallExpr>(S);
  if (!CallE)
    return false;

  const FunctionDecl *Callee = CallE->getDirectCallee();
  if (!Callee)
    return false;

  const auto *I = Callee->getIdentifier();
  if (!I)
    return false;

  if (I->getName() == "clang_analyzer_pset") {
    auto Loc = CallE->getLocStart();

    assert(CallE->getNumArgs() == 1 &&
           "clang_analyzer_pset takes one argument");

    const auto *DeclRef =
        dyn_cast<DeclRefExpr>(CallE->getArg(0)->IgnoreImpCasts());
    assert(DeclRef && "Argument to clang_analyzer_pset must be a DeclRefExpr");

    const auto *VD = dyn_cast<VarDecl>(DeclRef->getDecl());
    assert(VD &&
           "Argument to clang_analyzer_pset must be a reference to a VarDecl");

    diagPSet(VD, Loc);
    return true;
  } else if (I->getName() == "__lifetime_type_category") {
    auto Loc = CallE->getLocStart();

    auto QType = CallE->getDirectCallee()
                     ->getTemplateSpecializationInfo()
                     ->TemplateArguments->get(0)
                     .getAsType();
    TypeCategory TC = classifyTypeCategory(QType.getTypePtr());
    Reporter->debugTypeCategory(Loc, TC);
    return true;
  }
  return false;
}

// Update PSets in Builder through all CFGElements of this block
void PSetsBuilder::VisitBlock(const CFGBlock &B,
                              const LifetimeReporterBase *Reporter) {
  for (auto i = B.begin(); i != B.end(); ++i) {
    const CFGElement &E = *i;
    switch (E.getKind()) {
    case CFGElement::Statement: {
      const Stmt *S = E.castAs<CFGStmt>().getStmt();

      // Handle call to clang_analyzer_pset, which will print the pset of its
      // argument
      if (Reporter && HandleClangAnalyzerPset(S, Reporter))
        break;

      EvalStmt(S);

      // Kill all temporaries that vanish at the end of the full expression
      invalidateOwner(Owner::Temporary(), 0,
                      InvalidationReason::TemporaryLeftScope(S->getLocEnd()));
      break;
    }
    case CFGElement::LifetimeEnds: {
      auto Leaver = E.castAs<CFGLifetimeEnds>();

      // Stop tracking Pointers that leave scope
      erasePointer(Leaver.getVarDecl());

      // Invalidate all pointers that track leaving Owners
      invalidateOwner(
          Leaver.getVarDecl(), 0,
          InvalidationReason::PointeeLeftScope(
              Leaver.getTriggerStmt()->getLocEnd(), Leaver.getVarDecl()));
      break;
    }
    case CFGElement::NewAllocator:
    case CFGElement::AutomaticObjectDtor:
    case CFGElement::DeleteDtor:
    case CFGElement::BaseDtor:
    case CFGElement::MemberDtor:
    case CFGElement::TemporaryDtor:
    case CFGElement::Initializer:
    case CFGElement::ScopeBegin:
    case CFGElement::ScopeEnd:
    case CFGElement::LoopExit:
    case CFGElement::Constructor: // TODO
    case CFGElement::CXXRecordTypedCall:
      break;
    }
  }
}

class LifetimeContext {
  /// psets for a CFGBlock
  struct BlockContext {
    bool visited = false;
    /// Merged PSets of all predecessors of this CFGBlock
    PSetsMap EntryPSets;
    /// Computed PSets after updating EntryPSets through all CFGElements of
    /// this block
    PSetsMap ExitPSets;
  };

  ASTContext &ASTCtxt;
  LangOptions LangOptions;
  SourceManager &SourceMgr;
  CFG *ControlFlowGraph;
  const FunctionDecl *FuncDecl;
  std::vector<BlockContext> BlockContexts;
  AnalysisDeclContextManager AnalysisDCMgr;
  AnalysisDeclContext AC;
  LifetimeReporterBase &Reporter;

  bool computeEntryPSets(const CFGBlock &B, PSetsMap &EntryPSets);

  BlockContext &getBlockContext(const CFGBlock *B) {
    return BlockContexts[B->getBlockID()];
  }

  void dumpBlock(const CFGBlock &B) const {
    auto Loc = getStartLocOfBlock(B);
    llvm::errs() << "Block at " << SourceMgr.getBufferName(Loc) << ":"
                 << SourceMgr.getSpellingLineNumber(Loc) << "\n";
    B.dump(ControlFlowGraph, LangOptions, true);
  }

  void dumpCFG() const { ControlFlowGraph->dump(LangOptions, true); }

  PSetsBuilder
  createPSetsBuilder(PSetsMap &PSets,
                     const LifetimeReporterBase *Reporter = nullptr) {
    return PSetsBuilder(Reporter, ASTCtxt, PSets);
  }

  /// Approximate the SourceLocation of a Block for attaching pset debug
  /// diagnostics
  SourceLocation getStartLocOfBlock(const CFGBlock &B) const {
    if (&B == &ControlFlowGraph->getEntry())
      return FuncDecl->getLocStart();

    if (&B == &ControlFlowGraph->getExit())
      return FuncDecl->getLocEnd();

    for (const CFGElement &E : B) {
      switch (E.getKind()) {
      case CFGElement::Statement:
        return E.castAs<CFGStmt>().getStmt()->getLocStart();
      case CFGElement::LifetimeEnds:
        return E.castAs<CFGLifetimeEnds>().getTriggerStmt()->getLocEnd();
      default:;
      }
    }
    if (B.succ_empty())
      return SourceLocation();
    // for(auto i = B.succ_begin(); i != B.succ_end(); ++i)
    //{
    // TODO: this may lead to infinite recursion
    return getStartLocOfBlock(**B.succ_begin());
    //}
    llvm_unreachable("Could not determine start loc of CFGBlock");
  }

public:
  LifetimeContext(ASTContext &ASTCtxt, LifetimeReporterBase &Reporter,
                  SourceManager &SourceMgr, const FunctionDecl *FuncDecl)
      : ASTCtxt(ASTCtxt), LangOptions(ASTCtxt.getLangOpts()),
        SourceMgr(SourceMgr), FuncDecl(FuncDecl), AnalysisDCMgr(ASTCtxt),
        AC(&AnalysisDCMgr, FuncDecl), Reporter(Reporter) {
    // TODO: do not build own CFG here. Use the one from callee
    // AnalysisBasedWarnings::IssueWarnings
    AC.getCFGBuildOptions().PruneTriviallyFalseEdges = true;
    AC.getCFGBuildOptions().AddInitializers = true;
    AC.getCFGBuildOptions().AddLifetime = true;
    AC.getCFGBuildOptions().AddStaticInitBranches = true;
    AC.getCFGBuildOptions().AddCXXNewAllocator = true;
    // TODO AddTemporaryDtors
    // TODO AddEHEdges
    ControlFlowGraph = AC.getCFG();
    BlockContexts.resize(ControlFlowGraph->getNumBlockIDs());
  }

  void TraverseBlocks();
};

/// Computes entry psets of this block by merging exit psets
/// of all reachable predecessors.
/// Returns true if this block is reachable, i.e. one of it predecessors has
/// been visited.
bool LifetimeContext::computeEntryPSets(const CFGBlock &B,
                                        PSetsMap &EntryPSets) {
  // If no predecessors have been visited by now, this block is not
  // reachable
  bool isReachable = false;
  for (auto i = B.pred_begin(); i != B.pred_end(); ++i) {
    CFGBlock *PredBlock = i->getReachableBlock();
    if (!PredBlock)
      continue;

    auto &PredBC = getBlockContext(PredBlock);
    if (!PredBC.visited)
      continue; // Skip this back edge.

    isReachable = true;
    auto PredPSets = PredBC.ExitPSets;
    // If this block is only reachable from an IfStmt, modify pset according
    // to condition.
    if (auto TermStmt = PredBlock->getTerminator().getStmt()) {
      // first successor is the then-branch, second successor is the
      // else-branch.
      bool thenBranch = (PredBlock->succ_begin()->getReachableBlock() == &B);
      // TODO: also use for, do-while and while conditions
      if (auto If = dyn_cast<IfStmt>(TermStmt)) {
        assert(PredBlock->succ_size() == 2);
        auto Builder = createPSetsBuilder(PredPSets);
        Builder.UpdatePSetsFromCondition(If->getCond(), thenBranch,
                                         If->getCond()->getLocStart());
      } else if (const auto *CO = dyn_cast<ConditionalOperator>(TermStmt)) {
        auto Builder = createPSetsBuilder(PredPSets);
        Builder.UpdatePSetsFromCondition(CO->getCond(), thenBranch,
                                         CO->getCond()->getLocStart());
      }
    }
    if (EntryPSets.empty())
      EntryPSets = PredPSets;
    else {
      // Merge PSets with pred's PSets; TODO: make this efficient
      for (auto &i : EntryPSets) {
        auto &Pointer = i.first;
        auto &PS = i.second;
        auto j = PredPSets.find(Pointer);
        if (j == PredPSets.end()) {
          // The only reason that predecessors have PSets for different
          // variables is that some of them lazily added global variables
          // or member variables.
          // If a global pointer is not mentioned, its pset is implicitly
          // {(null), (static)}

          // OR there was a goto that stayed in the same scope but skipped
          // back over the initialization of this Pointer.
          // Then we don't care, because the variable will not be referenced in
          // the C++ code before it is declared.

          PS = Pointer.mayBeNull() ? PSet::validOwnerOrNull(Owner::Static())
                                   : PSet::validOwner(Owner::Static());
          continue;
        }
        if (PS == j->second)
          continue;

        PS.merge(j->second);
      }
    }
  }
  return isReachable;
}

/// Traverse all blocks of the CFG.
/// The traversal is repeated until the psets come to a steady state.
void LifetimeContext::TraverseBlocks() {
  const PostOrderCFGView *SortedGraph = AC.getAnalysis<PostOrderCFGView>();

  bool updated;
  do {
    updated = false;
    for (const auto *B : *SortedGraph) {
      auto &BC = getBlockContext(B);

      // The entry block introduces the function parameters into the psets
      if (B == &ControlFlowGraph->getEntry()) {
        if (BC.visited)
          continue;

        // ExitPSets are the function parameters.
        for (const ParmVarDecl *PVD : FuncDecl->parameters()) {
          if (classifyTypeCategory(PVD->getType().getTypePtr()) !=
              TypeCategory::Pointer)
            continue;
          Pointer P(PVD);
          // Parameters cannot be invalid (checked at call site).
          auto PS = P.mayBeNull() ? PSet::validOwnerOrNull(PVD, 1)
                                  : PSet::validOwner(PVD, 1);
          // Reporter.PsetDebug(PS, PVD->getLocEnd(), P.getValue());
          // PVD->dump();
          BC.ExitPSets.emplace(P, std::move(PS));
        }
        BC.visited = true;
        continue;
      }

      if (B == &ControlFlowGraph->getExit())
        continue;

      // compute entry psets of this block by merging exit psets
      // of all reachable predecessors.
      PSetsMap EntryPSets;
      bool isReachable = computeEntryPSets(*B, EntryPSets);
      if (!isReachable)
        continue;

      if (BC.visited && EntryPSets == BC.EntryPSets) {
        // Has been computed at least once and nothing changed; no need to
        // recompute.
        continue;
      }

      BC.EntryPSets = EntryPSets;
      BC.ExitPSets = BC.EntryPSets;
      auto Builder = createPSetsBuilder(BC.ExitPSets);
      Builder.VisitBlock(*B, nullptr);
      BC.visited = true;
      updated = true;
    }
  } while (updated);

  // Once more to emit diagnostics with final psets
  for (const auto *B : *SortedGraph) {
    auto &BC = getBlockContext(B);
    if (!BC.visited)
      continue;

    BC.ExitPSets = BC.EntryPSets;
    auto Builder = createPSetsBuilder(BC.ExitPSets, &Reporter);
    Builder.VisitBlock(*B, &Reporter);
  }
}

} // end unnamed namespace

/// Check that the function adheres to the lifetime profile
void runLifetimeAnalysis(const FunctionDecl *Func, ASTContext &Context,
                         SourceManager &SourceMgr,
                         LifetimeReporterBase &Reporter) {
  if (!Func->doesThisDeclarationHaveABody())
    return;

  LifetimeContext LC(Context, Reporter, SourceMgr, Func);
  LC.TraverseBlocks();
}

/// Check that each global variable is initialized to a pset of {static} and/or
/// {null}
void runLifetimeAnalysis(const VarDecl *VD, ASTContext &Context,
                         LifetimeReporterBase &Reporter) {

  auto P = Pointer::get(VD);
  if (!P.hasValue())
    return;

  PSetsMap PSets; // TODO remove me
  PSetsBuilder Builder(&Reporter, Context, PSets);
  Builder.EvalVarDecl(VD);
}

} // end namespace clang
