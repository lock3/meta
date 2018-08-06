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
#include "llvm/ADT/Statistic.h"
#include <algorithm>
#include <map>
#include <sstream>
#include <unordered_map>

#define DEBUG_TYPE "Lifetime Analysis"

STATISTIC(MaxIterations, "The maximum # of passes over the cfg");

namespace clang {
namespace {

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

  if (auto Cat = classifyStd(T))
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

/// A Variable can represent
/// - a local variable: Var contains a non-null VarDecl
/// - the this pointer: Var contains a null VarDecl
/// - a life-time extended temporary: Var contains a non-null
/// MaterializeTemporaryExpr
/// - a normal temporary: Var contains a null MaterializeTemporaryExpr
/// plus fields of them (in member FDs).
struct Variable {
  Variable(const VarDecl *VD) : Var(VD) {}
  Variable(const MaterializeTemporaryExpr *MT) : Var(MT) {}

  static Variable temporary() {
    return Variable(static_cast<const MaterializeTemporaryExpr *>(nullptr));
  }
  static Variable thisPointer() {
    return Variable(static_cast<const VarDecl *>(nullptr));
  }

  bool operator==(const Variable &O) const {
    return Var == O.Var && FDs == O.FDs;
  }

  bool operator!=(const Variable &O) const { return !(*this == O); }

  bool operator<(const Variable &O) const {
    if (Var != O.Var)
      return Var < O.Var;
    if (FDs.size() != O.FDs.size())
      return FDs.size() != O.FDs.size();

    for (auto i = FDs.begin(), j = O.FDs.begin(); i != FDs.end(); ++i, ++j) {
      if (*i != *j)
        return std::less<const FieldDecl *>()(*i, *j);
    }
    return false;
  }

  bool hasGlobalStorage() const {
    auto *VD = Var.dyn_cast<const VarDecl *>();
    if (!VD)
      return false;
    return VD->hasGlobalStorage();
  }

  bool trackPset() const {
    if (isThisPointer() || isTemporary())
      return false;
    auto Category = classifyTypeCategory(getType());
    return Category == TypeCategory::Pointer || Category == TypeCategory::Owner;
  }

  bool isMemberVariableOfEnclosingClass() const {
    return isThisPointer() && !FDs.empty();
  }

  /// Returns QualType of Variable or empty QualType if it refers to the 'this'
  /// pointer
  QualType getType() const {
    if (FDs.empty()) {
      if (auto *VD = Var.dyn_cast<const VarDecl *>())
        return VD->getType();
      if (auto *MT = Var.dyn_cast<const MaterializeTemporaryExpr *>())
        return MT->getType();
      return {}; // Refers to 'this' pointer
    }

    return FDs.back()->getType();
  }

  bool isThisPointer() const {
    return Var.is<const VarDecl *>() && !Var.get<const VarDecl *>();
  }

  bool isTemporary() const {
    return Var.is<const MaterializeTemporaryExpr *>() &&
           !Var.get<const MaterializeTemporaryExpr *>();
  }

  bool isLifetimeExtendedTemporary() const {
    return Var.is<const MaterializeTemporaryExpr *>() &&
           Var.get<const MaterializeTemporaryExpr *>();
  }

  // Is the pset of this Variable allowed to contain null?
  bool mightBeNull() const {
    if (isThisPointer())
      return false;
    // TODO: detect gsl::nullable / gsl::non_null
    return getType().getCanonicalType()->isPointerType();
  }

  bool isCategoryPointer() const {
    if (isThisPointer())
      return true;
    return classifyTypeCategory(getType()) == TypeCategory::Pointer;
  }

  // If FDs is empty, Var must be of class type and
  // FD must be a field within that class type.
  // If FDs is not empty, FDs.back() must be of class type
  // and FD must be a field within that class type.
  void addFieldRef(const FieldDecl *FD) { FDs.push_back(FD); }

  std::string getName() const {
    std::stringstream ss;
    if (Var.is<const MaterializeTemporaryExpr *>()) {
      auto *MD = Var.get<const MaterializeTemporaryExpr *>();
      if (MD) {
        ss << "(lifetime-extended temporary through ";
        if (MD->getExtendingDecl())
          ss << std::string(MD->getExtendingDecl()->getName()) << ")";
        else
          ss << "(unknown))";
      } else {
        ss << "(temporary)";
      }
    } else {
      auto *VD = Var.get<const VarDecl *>();
      ss << (VD ? std::string(VD->getName()) : "this");
    }

    for (auto *FD : FDs)
      ss << "." << std::string(FD->getName());
    return ss.str();
  }

  llvm::PointerUnion<const VarDecl *, const MaterializeTemporaryExpr *> Var;

  /// Possibly empty list of fields on Var
  /// first entry is the field on VD,
  /// next entry is the field inside there, etc.
  std::vector<const FieldDecl *> FDs;
};

/// The reason why a pset became invalid
/// Invariant: (Reason != POINTEE_LEFT_SCOPE || Pointee) && Loc.isValid()
// TODO: We should use source ranges rather than single locations
//       for user friendlyness.
class InvalidationReason {
  enum EReason {
    NOT_INITIALIZED,
    POINTEE_LEFT_SCOPE,
    TEMPORARY_LEFT_SCOPE,
    POINTER_ARITHMETIC,
    DEREFERENCED
  } Reason;

  const VarDecl *Pointee = nullptr;
  SourceLocation Loc;

  InvalidationReason(SourceLocation Loc, EReason Reason,
                     const VarDecl *Pointee = nullptr)
      : Reason(Reason), Pointee(Pointee), Loc(Loc) {
    assert(Loc.isValid());
  }

public:
  SourceLocation getLoc() const { return Loc; }

  void emitNote(const LifetimeReporterBase &Reporter) const {
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
    case POINTER_ARITHMETIC:
      Reporter.diag(Loc, diag::note_pointer_arithmetic);
      return;
    case DEREFERENCED:
      Reporter.diag(Loc, diag::note_dereferenced);
      return;
    }
    llvm_unreachable("Invalid InvalidationReason::Reason");
  }

  static InvalidationReason NotInitialized(SourceLocation Loc) {
    return {Loc, NOT_INITIALIZED};
  }

  static InvalidationReason PointeeLeftScope(SourceLocation Loc,
                                             const VarDecl *Pointee) {
    assert(Pointee);
    return {Loc, POINTEE_LEFT_SCOPE, Pointee};
  }

  static InvalidationReason TemporaryLeftScope(SourceLocation Loc) {
    return {Loc, TEMPORARY_LEFT_SCOPE};
  }

  static InvalidationReason PointerArithmetic(SourceLocation Loc) {
    return {Loc, POINTER_ARITHMETIC};
  }

  static InvalidationReason Dereferenced(SourceLocation Loc) {
    return {Loc, DEREFERENCED};
  }
};

/// The reason how null entered a pset
class NullReason {
  SourceLocation Loc;

public:
  NullReason(SourceLocation Loc) : Loc(Loc) { assert(Loc.isValid()); }
  void emitNote(const LifetimeReporterBase &Reporter) const {
    Reporter.diag(Loc, diag::note_null_here);
  }
};

/// A pset (points-to set) can contain
/// - null
/// - static
/// - invalid
/// - variables with an order
/// It a Pset contains non of that, its "unknown".
class PSet {
public:
  // Initializes an unknown pset
  PSet() : ContainsNull(false), ContainsInvalid(false), ContainsStatic(false) {}

  bool operator==(const PSet &O) const {
    return ContainsInvalid == O.ContainsInvalid &&
           ContainsNull == O.ContainsNull &&
           ContainsStatic == O.ContainsStatic && Vars == O.Vars;
  }

  void explainWhyInvalid(const LifetimeReporterBase &Reporter) const {
    for (auto &R : InvReasons)
      R.emitNote(Reporter);
  }

  void explainWhyNull(const LifetimeReporterBase &Reporter) const {
    for (auto &R : NullReasons)
      R.emitNote(Reporter);
  }

  bool isValid() const {
    return ContainsNull || ContainsStatic || !Vars.empty();
  }

  bool containsInvalid() const { return ContainsInvalid; }
  bool isInvalid() const {
    return !ContainsNull && !ContainsStatic && ContainsInvalid && Vars.empty();
  }

  bool isUnknown() const {
    return !ContainsInvalid && !ContainsNull && !ContainsStatic && Vars.empty();
  }

  /// Returns true if this pset contains Variable with the same or a lower order
  /// i.e. whether invalidating (Variable, order) would invalidate this pset
  bool contains(Variable Var, unsigned order = 0) const {
    auto i = Vars.find(Var);
    return i != Vars.end() && i->second >= order;
  }

  bool containsNull() const { return ContainsNull; }
  bool isNull() const {
    return ContainsNull && !ContainsStatic && !ContainsInvalid && Vars.empty();
  }

  void removeNull() { ContainsNull = false; }

  bool containsStatic() const { return ContainsStatic; }
  void addStatic() { ContainsStatic = true; }

  const std::map<Variable, unsigned> &vars() const { return Vars; }

  const std::vector<InvalidationReason> &invReasons() { return InvReasons; }
  const std::vector<NullReason> &nullReasons() { return NullReasons; }

  bool isSubstitutableFor(const PSet &O) {
    // If a includes invalid, then b must include invalid.
    if (ContainsInvalid && !O.ContainsInvalid)
      return false;

    // If a includes null, then b must include null.
    if (ContainsNull && !O.ContainsNull)
      return false;

    // If b includes static and no x or o, then a must include static and no x
    // or o.
    if (!ContainsStatic && O.ContainsStatic)
      return false;

    // If a includes o'', then b must include o'' or o'. (etc.)
    // TODO Optimize
    for (auto &kv : Vars) {
      auto &V = kv.first;
      auto Order = kv.second;
      auto i = O.Vars.find(V);
      if (i == O.Vars.end() || i->second > Order)
        return false;
    }

    // TODO
    // If a includes o'', then b must include o'' or o'. (etc.)
    // If a includes o', then b must include o'.
    // If a includes o, then b must include o or o' or o'' or some x with
    // lifetime less than o. If b includes one or more xb1..n, where xbshortest
    // is the one with shortest lifetime, then a must include static or xa1..m
    // where all have longer lifetime than xbshortest.
    return true;
  }

  std::string str() const {
    if (isUnknown())
      return "(unknown)";
    std::stringstream ss;
    int notFirst = 0;
    if (ContainsInvalid)
      ss << (notFirst++ ? ", " : "") << "(invalid)";
    if (ContainsNull)
      ss << (notFirst++ ? ", " : "") << "(null)";
    if (ContainsStatic)
      ss << (notFirst++ ? ", " : "") << "(static)";
    for (const auto &V : Vars) {
      ss << (notFirst++ ? ", " : "") << V.first.getName();
      for (size_t j = 0; j < V.second; ++j)
        ss << "'";
    }
    return ss.str();
  }

  void print(raw_ostream &Out) const { Out << str() << "\n"; }

  /// Merge contents of other pset into this
  void merge(const PSet &O) {
    if (!ContainsInvalid && O.ContainsInvalid) {
      ContainsInvalid = true;
      InvReasons = O.InvReasons;
    }

    if (!ContainsNull && O.ContainsNull) {
      ContainsNull = true;
      NullReasons = O.NullReasons;
    }
    ContainsStatic |= O.ContainsStatic;

    // TODO: optimize
    for (const auto &VO : O.Vars) {
      auto V = Vars.find(VO.first);
      if (V == Vars.end()) {
        Vars.insert(VO);
      } else {
        // if Vars contains obj' and otherPset.p contains obj''
        // then the union shall be invalidated whenever obj' or obj'' is
        // invalidated
        // which is the same as whenever obj'' is invalidated
        V->second = std::max(V->second, VO.second);
      }
    }
  }

  void insert(Variable Var, unsigned order = 0) {
    if (Var.hasGlobalStorage()) {
      ContainsStatic = true;
      return;
    }

    if (Vars.count(Var))
      order = std::min(Vars[Var], order);

    Vars[Var] = order;
  }

  void appendNullReason(NullReason R) { NullReasons.push_back(R); }

  void appendInvalidReason(InvalidationReason R) { InvReasons.push_back(R); }

  /// The pointer is dangling
  static PSet invalid(InvalidationReason Reason) {
    PSet ret;
    ret.ContainsInvalid = true;
    ret.InvReasons.push_back(Reason);
    return ret;
  }

  /// The pointer is dangling
  static PSet invalid(const std::vector<InvalidationReason> &Reasons) {
    PSet ret;
    ret.ContainsInvalid = true;
    ret.InvReasons = Reasons;
    return ret;
  }

  /// A pset that contains only (null)
  static PSet null(NullReason Reason) {
    PSet ret;
    ret.ContainsNull = true;
    ret.NullReasons.push_back(Reason);
    return ret;
  }

  /// A pset that contains only (static)
  static PSet onlyStatic() {
    PSet ret;
    ret.ContainsStatic = true;
    return ret;
  }

  /// A pset that contains (static), (null)
  static PSet staticOrNull() {
    PSet ret;
    ret.ContainsStatic = true;
    ret.ContainsNull = true;
    return ret;
  }

  /// The pset contains one of obj, obj' or obj''
  static PSet pointsToVariable(Variable Var, unsigned order = 0) {
    PSet ret;
    if (Var.hasGlobalStorage())
      ret.ContainsStatic = true;
    else
      ret.Vars.emplace(Var, order);
    return ret;
  }

  /// The pset contains null and one of obj, obj' or obj''
  static PSet pointsToVariableOrNull(Variable Var, unsigned order = 0) {
    PSet ret = pointsToVariable(Var, order);
    ret.ContainsNull = true;
    return ret;
  }

private:
  int ContainsNull : 1;
  int ContainsInvalid : 1;
  int ContainsStatic : 1;
  /// Maps Variable obj to order.
  /// If Variable is not an Owner, order must be zero
  /// (obj,0) == obj: points to obj
  /// (obj,1) == obj': points to object owned directly by obj
  /// (obj,2) == obj'': points an object kept alive indirectly (transitively)
  /// via owner obj
  std::map<Variable, unsigned> Vars;

  std::vector<InvalidationReason> InvReasons;
  std::vector<NullReason> NullReasons;
};

// TODO optimize (sorted vector?)
using PSetsMap = std::map<Variable, PSet>;

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

  // Returns the variable which the expression refers to,
  // or None.
  Variable refersTo(const Expr *E) {
    E = E->IgnoreParenCasts();
    switch (E->getStmtClass()) {
    case Expr::DeclRefExprClass: {
      const auto *DeclRef = cast<DeclRefExpr>(E);
      auto *VD = dyn_cast<VarDecl>(DeclRef->getDecl());
      if (VD)
        return Variable(VD);
      return Variable::temporary();
    }

    case Expr::CXXThisExprClass: {
      return Variable::thisPointer();
    }

    case Expr::MemberExprClass: {
      const auto *M = cast<MemberExpr>(E);
      auto SubV = refersTo(M->getBase());

      /// The returned declaration will be a FieldDecl or (in C++) a VarDecl
      /// (for static data members), a CXXMethodDecl, or an EnumConstantDecl.
      auto *FD = dyn_cast<FieldDecl>(M->getMemberDecl());
      if (FD) {
        SubV.addFieldRef(FD);
        return SubV;
      }
      return Variable::temporary();
    }
    default:
      return Variable::temporary();
    }
  }

  // Evaluate the given expression for all effects on psets of variables
  void EvalExpr(const Expr *E) {
    E = E->IgnoreParenCasts();

    switch (E->getStmtClass()) {
    case Expr::BinaryOperatorClass: {
      const auto *BinOp = cast<BinaryOperator>(E);
      if (BinOp->getOpcode() == BO_Assign) {
        EvalExpr(BinOp->getLHS()); // Eval for side-effects
        auto Category = classifyTypeCategory(BinOp->getLHS()->getType());
        Variable V = refersTo(BinOp->getLHS());
        if (Category == TypeCategory::Owner) {
          // When an Owner x is copied to or moved to, set pset(x) = {x'}
          SetPSet(V, PSet::pointsToVariable(V, 1), BinOp->getExprLoc());
        } else if (Category == TypeCategory::Pointer) {
          // This assignment updates a Pointer
          PSet PS = EvalExprForPSet(BinOp->getRHS(), false);
          SetPSet(V, PS, BinOp->getExprLoc());
        }
        return;
      } else if (BinOp->getLHS()->getType()->isPointerType() &&
                 (BinOp->getOpcode() == BO_AddAssign ||
                  BinOp->getOpcode() == BO_SubAssign)) {
        // Affects pset; forbidden by the bounds profile.
        Variable V = refersTo(BinOp->getLHS());
        SetPSet(V,
                PSet::invalid(
                    InvalidationReason::PointerArithmetic(BinOp->getExprLoc())),
                BinOp->getExprLoc());
        // TODO: diagnose even if we have no VarDecl?
      }
      break;
    }
    case Expr::UnaryOperatorClass: {
      const auto *UnOp = cast<UnaryOperator>(E);
      auto *SubExpr = UnOp->getSubExpr();
      if (SubExpr->getType()->isPointerType()) {
        switch (UnOp->getOpcode()) {
        case UO_Deref: {
          // Check if dereferencing this pointer is valid
          PSet PS = EvalExprForPSet(SubExpr, false);
          CheckPSetValidity(PS, UnOp->getExprLoc());
          return;
        }
        case UO_PostInc:
        case UO_PostDec:
        case UO_PreInc:
        case UO_PreDec: {
          // Affects pset; forbidden by the bounds profile.
          if (Optional<Variable> V = refersTo(SubExpr)) {
            SetPSet(*V,
                    PSet::invalid(InvalidationReason::PointerArithmetic(
                        UnOp->getExprLoc())),
                    UnOp->getExprLoc());
          }
          // TODO: diagnose even if we have no VarDecl?
          break;
        }
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
    case Expr::CXXOperatorCallExprClass:
    case Expr::CXXMemberCallExprClass:
    case Expr::CallExprClass: {
      EvalCallExpr(cast<CallExpr>(E));
      return;
    }
    case Expr::ConditionalOperatorClass:
      // Do not handle it here; this is always the terminator of a CFG block
      // and it will be handled explicitly there.
      return;
    default:;
    }
    // Other Expr, just recurse
    for (const Stmt *SubStmt : E->children())
      EvalStmt(SubStmt);
  }

  struct CallArgument {
    CallArgument(const Expr *ArgumentExpr, PSet PS)
        : ArgumentExpr(ArgumentExpr), PS(std::move(PS)) {}
    const Expr *ArgumentExpr;
    PSet PS;
  };

  struct CallArguments {
    std::vector<CallArgument> Oin_strong;
    std::vector<CallArgument> Oin_weak;
    std::vector<CallArgument> Oin;
    std::vector<CallArgument> Pin;
    // The pointee is the output
    std::vector<const Expr *> Pout;
  };

  void PushCallArguments(const Expr *Arg, QualType ParamQType,
                         CallArguments &Args) {
    // TODO implement gsl::lifetime annotations
    // TODO implement aggregates
    const Type *ParamType = ParamQType->getUnqualifiedDesugaredType();

    if (auto *R = dyn_cast<ReferenceType>(ParamType)) {
      if (classifyTypeCategory(R->getPointeeType()) == TypeCategory::Owner) {
        if (isa<RValueReferenceType>(R)) {
          Args.Oin_strong.emplace_back(
              Arg, EvalExprForPSet(Arg, /*referenceCtx=*/false));
        } else if (ParamQType.isConstQualified()) {
          Args.Oin_weak.emplace_back(
              Arg, EvalExprForPSet(Arg, /*referenceCtx=*/false));
        } else {
          Args.Oin.emplace_back(Arg,
                                EvalExprForPSet(Arg, /*referenceCtx=*/false));
        }
      } else {
        // Type Category is Pointer due to raw references.
        Args.Pin.emplace_back(Arg, EvalExprForPSet(Arg, /*referenceCtx=*/true));
      }

    } else if (classifyTypeCategory(ParamQType) == TypeCategory::Pointer) {
      Args.Pin.emplace_back(Arg, EvalExprForPSet(Arg, /*referenceCtx=*/false));
    } else {
      EvalExpr(Arg);
    }

    // A “function output” means a return value or a parameter passed by raw
    // pointer to non-const (and is not considered to include the top-level raw
    // pointer, because the output is the pointee).
    if (ParamQType->isPointerType()) {
      auto Pointee = ParamType->getPointeeType();
      if (!Pointee.isConstQualified() &&
          classifyTypeCategory(Pointee) == TypeCategory::Pointer)
        Args.Pout.push_back(Arg);
    }
  }

  struct CallTypes {
    const FunctionProtoType *FTy = nullptr;
    const CXXRecordDecl *ClassDecl = nullptr;
  };

  /// Obtains the function prototype (without 'this' pointer) and the type of
  /// the object (if MemberCallExpr).
  CallTypes GetCallTypes(const Expr *CalleeE) {
    CallTypes CT;

    if (CalleeE->hasPlaceholderType(BuiltinType::BoundMember)) {
      CalleeE = CalleeE->IgnoreParenImpCasts();
      if (const auto *BinOp = dyn_cast<BinaryOperator>(CalleeE)) {
        auto MemberPtr =
            BinOp->getRHS()->getType()->castAs<MemberPointerType>();
        CT.FTy = dyn_cast<FunctionProtoType>(
            MemberPtr->getPointeeType().IgnoreParens().getTypePtr());
        CT.ClassDecl = MemberPtr->getClass()->getAsCXXRecordDecl();
        assert(CT.FTy);
        assert(CT.ClassDecl);
        return CT;
      }

      if (const auto *ME = dyn_cast<MemberExpr>(CalleeE)) {
        CT.FTy = dyn_cast<FunctionProtoType>(ME->getMemberDecl()->getType());
        auto ClassType = ME->getBase()->getType();
        if (ClassType->isPointerType())
          ClassType = ClassType->getPointeeType();
        CT.ClassDecl = ClassType->getAsCXXRecordDecl();
        assert(CT.FTy);
        assert(CT.ClassDecl);
        return CT;
      }

      CalleeE->dump();
      llvm_unreachable("not a binOp after boundMember");
    }

    auto *P = dyn_cast<PointerType>(
        CalleeE->getType()->getUnqualifiedDesugaredType());
    assert(P);
    CT.FTy = dyn_cast<FunctionProtoType>(
        P->getPointeeType()->getUnqualifiedDesugaredType());

    assert(CT.FTy);
    return CT;
  }

  /// Returns the psets of each expressions in PinArgs,
  /// plus the psets of dereferencing each pset further.
  std::vector<CallArgument>
  diagnoseAndExpandPin(const std::vector<CallArgument> &PinArgs) {
    std::vector<CallArgument> PinExtended;
    // llvm::errs() << "Start Pin: ";
    for (auto &CA : PinArgs) {
      const Expr *ArgExpr = CA.ArgumentExpr;
      PSet PS = CA.PS;
      PinExtended.emplace_back(ArgExpr, PS);

      if (PS.containsInvalid()) {
        if (Reporter) {
          Reporter->warnParameterDangling(ArgExpr->getExprLoc(),
                                          /*indirectly=*/false);
          PS.explainWhyInvalid(*Reporter);
        }
        break;
      }
      // For each Pointer parameter p, treat it as if it were additionally
      // followed by a generated deref__p parameter of the same type as *p. If
      // deref_p is also a Pointer, treat it as if it were additionally followed
      // by a generated deref__deref__p parameter of type **p. These are treated
      // as distinct parameters in the following.
      // TODO: What is the type of `*p` when p is of class-type, possibly
      // without operator*?
      QualType QT = ArgExpr->getType()->getPointeeType();
      // llvm::errs() << "param with PSET " << PS.str() << "\n";
      while (!QT.isNull() && QT->isPointerType() && !PS.containsInvalid()) {
        PS = derefPSet(PS, ArgExpr->getExprLoc());
        if (PS.containsInvalid()) {
          if (Reporter) {
            Reporter->warnParameterDangling(ArgExpr->getExprLoc(),
                                            /*indirectly=*/true);
            PS.explainWhyInvalid(*Reporter);
          }
          break;
        }
        // llvm::errs() << " deref -> " << PS.str() << "\n";
        PinExtended.emplace_back(ArgExpr, PS);
        QT = QT->getPointeeType();
      }
    }
    return PinExtended;
  }

  /// Diagnose if psets arguments in Oin and Pin refer to the same variable
  void diagnoseParameterAliasing(const std::vector<CallArgument> &Pin,
                                 const std::vector<CallArgument> &Oin) {
    if (!Reporter)
      return;
    std::map<Variable, SourceLocation> AllVars;
    for (auto &CA : Pin) {
      for (auto &KV : CA.PS.vars()) {
        auto &Var = KV.first;
        // pset(argument(p)) and pset(argument(x)) must be disjoint (as long as
        // not annotated)
        auto i = AllVars.emplace(Var, CA.ArgumentExpr->getExprLoc());
        if (!i.second) {
          Reporter->warnParametersAlias(CA.ArgumentExpr->getExprLoc(),
                                        i.first->second, Var.getName());
        }
      }
    }
    for (auto &CA : Oin) {
      for (auto &KV : CA.PS.vars()) {
        auto &Var = KV.first;
        // pset(argument(p)) and pset(argument(x)) must be disjoint (as long
        // as not annotated) Enforce that pset() of each argument does not
        // refer to a local Owner in Oin
        auto i = AllVars.emplace(Var, CA.ArgumentExpr->getExprLoc());
        if (!i.second) {
          Reporter->warnParametersAlias(CA.ArgumentExpr->getExprLoc(),
                                        i.first->second, Var.getName());
        }
      }
    }

    /*llvm::errs() << "Vars passed into call: ";
    for (auto &KV : AllVars) {
      llvm::errs() << " " << KV.first.getName();
    }
    llvm::errs() << "\n";*/
  }

  /// Evaluates the CallExpr for effects on psets.
  /// When a non-const pointer to pointer or reference to pointer is passed
  /// into a function, it's pointee's are invalidated.
  /// Returns true if CallExpr was handled.
  PSet EvalCallExpr(const CallExpr *CallE) {
    // Handle call to clang_analyzer_pset, which will print the pset of its
    // argument
    if (HandleClangAnalyzerPset(CallE))
      return {};

    auto *CalleeE = CallE->getCallee();
    EvalExpr(CalleeE);
    CallTypes CT = GetCallTypes(CalleeE);
    CallArguments Args;
    for (unsigned i = 0; i < CallE->getNumArgs(); ++i) {
      const Expr *Arg = CallE->getArg(i);

      QualType ParamQType = [&] {
        // For CXXOperatorCallExpr, getArg(0) is the 'this' pointer.
        if (dyn_cast<CXXOperatorCallExpr>(CallE)) {
          if (i == 0) {
            auto QT = ASTCtxt.getLValueReferenceType(Arg->getType(),
                                                     /*SpelledAsLValue=*/true);
            if (CT.FTy->isConst())
              QT.addConst();
            return QT;
          } else
            return CT.FTy->getParamType(i - 1);
        }
        if (CT.FTy->isVariadic())
          return Arg->getType();
        else
          return CT.FTy->getParamType(i);
      }();

      PushCallArguments(Arg, ParamQType,
                        Args); // appends into Args
    }

    if (CT.ClassDecl) {
      // A this pointer parameter is treated as if it were declared as a
      // reference to the current object
      auto *MemberCall = dyn_cast<CXXMemberCallExpr>(CallE);
      if (MemberCall) {
        auto *Object = MemberCall->getImplicitObjectArgument();
        assert(Object);

        // TODO: *this is gsl::non_null annotated
        auto LValRefToObject = ASTCtxt.getLValueReferenceType(
            Object->getType(), /*SpelledAsLValue=*/true);
        if (CT.FTy->isConst())
          LValRefToObject.addConst();

        PushCallArguments(Object, LValRefToObject,
                          Args); // appends into Args
      }
    }

    // TODO If p is annotated [[gsl::lifetime(x)]], then ensure that pset(p)
    // == pset(x)

    std::vector<CallArgument> PinExtended = diagnoseAndExpandPin(Args.Pin);
    diagnoseParameterAliasing(PinExtended, Args.Oin);

    // If p is explicitly lifetime-annotated with x, then each call site
    // enforces the precondition that argument(p) is a valid Pointer and
    // pset(argument(p)) == pset(argument(x)), and in the callee on function
    // entry set pset(p) = pset(x).

    // Enforce that pset() of each argument does not refer to a non-const
    // global Owner
    PSet ret;
    for (CallArgument &CA : PinExtended) {
      // llvm::errs() << " Pin PS:" << CA.PS.str() << "\n";
      ret.merge(CA.PS);
    }
    for (CallArgument &CA : Args.Oin) {
      // llvm::errs() << " Oin PS:" << CA.PS.str() << "\n";
      ret.merge(CA.PS);
    }
    return ret;
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
        return PSet::pointsToVariable(MaterializeTemporaryE, 0);
      else
        return PSet::pointsToVariable(Variable::temporary(), 0);
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
            return PSet::pointsToVariable(VD);
        } else {
          if (VD->getType()->isArrayType())
            // Forbidden by bounds profile
            // Alternative would be: T a[]; auto *p = a; -> pset_ptr(p) =
            // {a}
            return PSet::invalid(
                InvalidationReason::PointerArithmetic(DeclRef->getLocation()));
          else
            // T i; auto *p = i; -> pset_ptr(p) = pset(i)
            return GetPSet(VD);
        }
      }
      return {};
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
          return PSet::pointsToVariable(VD, 0);
      }
      return {};
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
        // The returned declaration will be a FieldDecl or (in C++) a
        // VarDecl (for static data members), a CXXMethodDecl, or an
        // EnumConstantDecl.
        if (auto *FD = dyn_cast<FieldDecl>(MemberE->getMemberDecl())) {
          auto V = Variable::thisPointer();
          V.addFieldRef(FD);
          return PSet::pointsToVariable(V);
        } else if (auto *VD = dyn_cast<VarDecl>(MemberE->getMemberDecl())) {
          // A static data member of this class
          return PSet::onlyStatic();
        }
      }

      return EvalExprForPSet(
          Base, !Base->getType().getCanonicalType()->isPointerType());
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
          return derefPSet(PS, UnOp->getOperatorLoc());
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
        // TODO: diagnose
        return PSet{};
      default:
        return EvalExprForPSet(CastE->getSubExpr(), referenceCtx);
      }
      break;
    }
    case Expr::InitListExprClass: {
      const auto *I = cast<InitListExpr>(E);
      if (I->isSyntacticForm())
        I = I->getSemanticForm();

      if (I->getType()->isPointerType() && I->getNumInits() == 0)
        return PSet::null(I->getLocStart());

      if (I->getNumInits() == 1)
        return EvalExprForPSet(I->getInit(0), referenceCtx);

      return PSet{};
    }
    case Expr::CXXConstructExprClass:
      return PSet::null(E->getExprLoc());

    case Expr::CXXOperatorCallExprClass:
    case Expr::CXXMemberCallExprClass:
    case Expr::CallExprClass: {
      return EvalCallExpr(cast<CallExpr>(E));
    }
    default:;
    }

    if (E->isNullPointerConstant(ASTCtxt, Expr::NPC_ValueDependentIsNotNull)) {
      if (referenceCtx) {
        // It is illegal to bind a reference to null
        return PSet::invalid(
            InvalidationReason::NotInitialized(E->getExprLoc()));
      } else {
        return PSet::null(E->getExprLoc());
      }
    }

    // Unhandled case
    EvalExpr(E);
    return {};
  }

  void CheckPSetValidity(const PSet &PS, SourceLocation Loc,
                         bool flagNull = true);

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
  void invalidateOwner(Variable O, unsigned order, InvalidationReason Reason) {
    for (auto &i : PSets) {
      const auto &Pointer = i.first;
      PSet &PS = i.second;
      if (!PS.isValid())
        continue; // Nothing to invalidate

      if (PS.contains(O, order))
        SetPSet(Pointer, PSet::invalid(Reason), Reason.getLoc());
    }
  }
  void erasePointer(Variable P) { PSets.erase(P); }
  PSet GetPSet(Variable P);
  void SetPSet(Variable P, PSet PS, SourceLocation Loc);
  PSet derefPSet(PSet P, SourceLocation Loc);

  void diagPSet(Variable P, SourceLocation Loc) {
    auto i = PSets.find(P);
    if (i != PSets.end())
      Reporter->debugPset(Loc, P.getName(), i->second.str());
    else
      Reporter->debugPset(Loc, P.getName(), "(untracked)");
  }

  bool HandleClangAnalyzerPset(const CallExpr *CallE);

public:
  PSetsBuilder(const LifetimeReporterBase *Reporter, ASTContext &ASTCtxt,
               PSetsMap &PSets)
      : Reporter(Reporter), ASTCtxt(ASTCtxt), PSets(PSets) {}

  void EvalVarDecl(const VarDecl *VD) {
    const Expr *Initializer = VD->getInit();
    SourceLocation Loc = VD->getLocEnd();

    switch (classifyTypeCategory(VD->getType())) {
    case TypeCategory::Pointer: {
      PSet PS;
      if (Initializer)
        PS = EvalExprForPSet(
            Initializer, !VD->getType().getCanonicalType()->isPointerType());
      else if (Loc.isValid())
        PS = PSet::invalid(InvalidationReason::NotInitialized(Loc));

      SetPSet(VD, PS, Loc);
      break;
    }
    case TypeCategory::Owner: {
      SetPSet(VD, PSet::pointsToVariable(VD, 1), Loc);
    } // fallthrough
    default: {
      if (Initializer)
        EvalExpr(Initializer);
    }
    }
  }

  void VisitBlock(const CFGBlock &B, const LifetimeReporterBase *Reporter);

  void UpdatePSetsFromCondition(const Stmt *S, bool Positive,
                                SourceLocation Loc);
};

// Manages lifetime information for the CFG of a FunctionDecl
PSet PSetsBuilder::GetPSet(Variable P) {
  auto i = PSets.find(P);
  if (i != PSets.end())
    return i->second;

  // Assumption: global Pointers have a pset of {static, null}
  if (P.hasGlobalStorage() || P.isMemberVariableOfEnclosingClass()) {
    if (P.mightBeNull())
      return PSet::staticOrNull();
    return PSet::onlyStatic();
  }

  llvm::errs() << "PSetsBuilder::GetPSet: did not find pset for " << P.getName()
               << "\n";
  llvm_unreachable("Missing pset for Pointer");
}

/// Computes the pset of dereferencing a variable with the given pset
/// If PS contains (null), it is silently ignored.
PSet PSetsBuilder::derefPSet(PSet PS, SourceLocation Loc) {
  // When a local Pointer p is dereferenced using unary * or -> to create a
  // temporary tmp, then if pset(pset(p)) is nonempty, set pset(tmp) =
  // pset(pset(p)) and Kill(pset(tmp)'). Otherwise, set pset(tmp) = {tmp}.
  if (PS.isUnknown())
    return {};

  if (PS.containsInvalid()) {
    std::vector<InvalidationReason> invReasons = PS.invReasons();
    invReasons.emplace_back(InvalidationReason::Dereferenced(Loc));
    return PSet::invalid(PS.invReasons());
  }

  PSet RetPS;
  if (PS.containsStatic())
    RetPS.addStatic();

  for (auto &KV : PS.vars()) {
    const Variable &V = KV.first;
    auto order = KV.second;

    if (order > 0)
      RetPS.insert(V, order + 1); // pset(o') = { o'' }
    else if (V.isCategoryPointer())
      RetPS.merge(GetPSet(V));
  }

  if (RetPS.containsNull())
    RetPS.appendNullReason(Loc);

  if (RetPS.containsInvalid())
    RetPS.appendInvalidReason(InvalidationReason::Dereferenced(Loc));

  return RetPS;
}

void PSetsBuilder::SetPSet(Variable P, PSet PS, SourceLocation Loc) {
  assert(P.getName() != "");
  if (!P.trackPset())
    return;

  // Assumption: global Pointers have a pset that is a subset of {static,
  // null}
  if (P.hasGlobalStorage() && !PS.isUnknown() &&
      !PS.isSubstitutableFor(PSet::staticOrNull()) && Reporter)
    Reporter->warnPsetOfGlobal(Loc, P.getName(), PS.str());

  auto i = PSets.find(P);
  if (i != PSets.end())
    i->second = std::move(PS);
  else
    PSets.emplace(P, PS);
}

void PSetsBuilder::CheckPSetValidity(const PSet &PS, SourceLocation Loc,
                                     bool flagNull) {
  if (!Reporter)
    return;

  if (PS.isUnknown()) {
    Reporter->diag(Loc, diag::warn_deref_unknown);
    return;
  }

  if (PS.containsInvalid()) {
    Reporter->warnDerefDangling(Loc, !PS.isInvalid());
    PS.explainWhyInvalid(*Reporter);
    return;
  }

  if (PS.containsNull()) {
    Reporter->warnDerefNull(Loc, !PS.isNull());
    return;
  }
}

/// Updates psets to remove 'null' when entering conditional statements. If
/// 'positive' is false, handles expression as-if it was negated.
/// Examples:
///   int* p = f();
/// if(p)
///  ... // pset of p does not contain 'null'
/// else
///  ... // pset of p is 'null'
/// if(!p)
///  ... // pset of p is 'null'
/// else
///  ... // pset of p does not contain 'null'
void PSetsBuilder::UpdatePSetsFromCondition(const Stmt *S, bool Positive,
                                            SourceLocation Loc) {
  const auto *E = dyn_cast_or_null<Expr>(S);
  if (!E)
    return;
  E = E->IgnoreParenImpCasts();
  if (const auto *UO = dyn_cast<UnaryOperator>(E)) {
    if (UO->getOpcode() != UO_LNot)
      return;
    E = UO->getSubExpr();
    UpdatePSetsFromCondition(E, !Positive, E->getLocStart());
    return;
  }
  if (const auto *BO = dyn_cast<BinaryOperator>(E)) {
    BinaryOperator::Opcode OC = BO->getOpcode();
    if (OC != BO_NE && OC != BO_EQ)
      return;
    // The p == null is the negative case.
    if (OC == BO_EQ)
      Positive = !Positive;
    const auto *LHS = BO->getLHS()->IgnoreParenImpCasts();
    const auto *RHS = BO->getRHS()->IgnoreParenImpCasts();
    Optional<Variable> LHSVar = refersTo(LHS);
    Optional<Variable> RHSVar = refersTo(RHS);
    bool LHSIsNull =
        LHS->isNullPointerConstant(
            ASTCtxt, Expr::NPC_ValueDependentIsNotNull) != Expr::NPCK_NotNull;
    bool RHSIsNull =
        RHS->isNullPointerConstant(
            ASTCtxt, Expr::NPC_ValueDependentIsNotNull) != Expr::NPCK_NotNull;
    if (LHSIsNull || (LHSVar && GetPSet(*LHSVar).isNull()))
      E = RHS;
    else if (RHSIsNull || (RHSVar && GetPSet(*RHSVar).isNull()))
      E = LHS;
    else
      return;
  }

  if (Optional<Variable> V = refersTo(E)) {
    if (V->trackPset()) {
      auto PS = GetPSet(*V);
      if (Positive)
        PS.removeNull();
      else
        PS = PSet::null(Loc);
      SetPSet(*V, PS, Loc);
    }
  }
}

/// Checks if the statement S is a call to clang_analyzer_pset and, if yes,
/// diags the pset of its argument
bool PSetsBuilder::HandleClangAnalyzerPset(const CallExpr *CallE) {

  const FunctionDecl *Callee = CallE->getDirectCallee();
  if (!Callee)
    return false;

  const auto *I = Callee->getIdentifier();
  if (!I)
    return false;

  if (I->getName() == "clang_analyzer_pset") {
    if (Reporter) {
      auto Loc = CallE->getLocStart();

      assert(CallE->getNumArgs() == 1 &&
             "clang_analyzer_pset takes one argument");

      // TODO: handle MemberExprs.
      const auto *DeclRef =
          dyn_cast<DeclRefExpr>(CallE->getArg(0)->IgnoreImpCasts());
      assert(DeclRef &&
             "Argument to clang_analyzer_pset must be a DeclRefExpr");

      const auto *VD = dyn_cast<VarDecl>(DeclRef->getDecl());
      assert(VD && "Argument to clang_analyzer_pset must be a reference to "
                   "a VarDecl");

      diagPSet(VD, Loc);
    }
    return true;
  } else if (I->getName() == "__lifetime_type_category") {
    if (Reporter) {
      auto Loc = CallE->getLocStart();

      auto Args = Callee->getTemplateSpecializationArgs();
      auto QType = Args->get(0).getAsType();
      TypeCategory TC = classifyTypeCategory(QType);
      Reporter->debugTypeCategory(Loc, TC);
    }
    return true;
  } else if (I->getName() == "__lifetime_type_category_arg") {
    if (Reporter) {
      auto Loc = CallE->getLocStart();
      auto QType = CallE->getArg(0)->getType();
      TypeCategory TC = classifyTypeCategory(QType);
      Reporter->debugTypeCategory(Loc, TC);
    }
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
      EvalStmt(S);

      // Kill all temporaries that vanish at the end of the full expression
      invalidateOwner(Variable::temporary(), 0,
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
  LangOptions LangOpts;
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
    B.dump(ControlFlowGraph, LangOpts, true);
  }

  void dumpCFG() const { ControlFlowGraph->dump(LangOpts, true); }

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
      : ASTCtxt(ASTCtxt), LangOpts(ASTCtxt.getLangOpts()), SourceMgr(SourceMgr),
        FuncDecl(FuncDecl), AnalysisDCMgr(ASTCtxt),
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

static const Stmt *getRealTerminator(const CFGBlock *B) {
  const Stmt *LastCFGStmt = nullptr;
  for (const CFGElement &Element : *B) {
    if (auto CFGSt = Element.getAs<CFGStmt>()) {
      LastCFGStmt = CFGSt->getStmt();
    }
  }
  const Stmt *TerminatorCond = B->getTerminatorCondition(true);
  if (TerminatorCond && isa<BinaryOperator>(TerminatorCond))
    return LastCFGStmt;
  else
    return TerminatorCond;
}

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
    // Unfortunately, PredBlock->getTerminatorCondition(true) is almost what
    // we whant here but not quite. In case of A || B, for the basic block
    // corresponding to B, the terminator expression is the whole
    // A || B. Is this a bug?
    if (auto TermCond = getRealTerminator(PredBlock)) {
      // First successor is the then-branch, second successor is the
      // else-branch.
      bool IsThenBranch = PredBlock->succ_begin()->getReachableBlock() == &B;
      auto Builder = createPSetsBuilder(PredPSets);
      Builder.UpdatePSetsFromCondition(TermCond, IsThenBranch,
                                       TermCond->getLocStart());
    }
    if (EntryPSets.empty())
      EntryPSets = PredPSets;
    else {
      // Merge PSets with pred's PSets; TODO: make this efficient
      for (auto &i : EntryPSets) {
        auto &Var = i.first;
        auto &PS = i.second;
        auto j = PredPSets.find(Var);
        if (j == PredPSets.end()) {
          // The only reason that predecessors have PSets for different
          // variables is that some of them lazily added global variables
          // or member variables.
          // If a global pointer is not mentioned, its pset is implicitly
          // {(null), (static)}

          // OR there was a goto that stayed in the same scope but skipped
          // back over the initialization of this Pointer.
          // Then we don't care, because the variable will not be referenced
          // in the C++ code before it is declared.

          PS = Var.mightBeNull() ? PSet::staticOrNull() : PSet::onlyStatic();
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
  static const unsigned IterationLimit = 128;

  bool Updated;
  unsigned IterationCount = 0;
  do {
    Updated = false;
    for (const auto *B : *SortedGraph) {
      auto &BC = getBlockContext(B);

      // The entry block introduces the function parameters into the psets
      if (B == &ControlFlowGraph->getEntry()) {
        if (BC.visited)
          continue;

        // ExitPSets are the function parameters.
        for (const ParmVarDecl *PVD : FuncDecl->parameters()) {
          if (classifyTypeCategory(PVD->getType()) != TypeCategory::Pointer)
            continue;
          Variable P(PVD);
          // Parameters cannot be invalid (checked at call site).
          auto PS = P.mightBeNull() ? PSet::pointsToVariableOrNull(P, 0)
                                    : PSet::pointsToVariable(P, 0);
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
      Updated = true;
    }
    ++IterationCount;
  } while (Updated && IterationCount < IterationLimit);

  if (IterationCount > MaxIterations)
    MaxIterations = IterationCount;

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

/// Check that each global variable is initialized to a pset of {static}
/// and/or {null}
void runLifetimeAnalysis(const VarDecl *VD, ASTContext &Context,
                         LifetimeReporterBase &Reporter) {

  if (classifyTypeCategory(VD->getType()) != TypeCategory::Pointer)
    return;

  PSetsMap PSets; // TODO remove me
  PSetsBuilder Builder(&Reporter, Context, PSets);
  Builder.EvalVarDecl(VD);
  // TODO
  // We don't track the PSets of variables with global storage; just make
  // sure that its pset is always {static} and/or {null}
  // if (!PS.isSubsetOf(PSet::validOwnerOrNull(Owner::Static())) && Reporter)
  // Reporter->warnPsetOfGlobal(Loc, P->getName(), PS.str());
}

} // namespace clang
