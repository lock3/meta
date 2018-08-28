//=- LifetimePset.h - Diagnose lifetime violations -*- C++ -*-================//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMEPSET_H
#define LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMEPSET_H

#include "clang/AST/DeclCXX.h"
#include "clang/AST/ExprCXX.h"
#include "clang/Analysis/Analyses/LifetimeTypeCategory.h"
#include <map>
#include <sstream>
#include <vector>

namespace clang {
namespace lifetime {
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

  bool isBaseEqual(const Variable &O) const { return Var == O.Var; }

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

  /// Returns QualType of Variable or empty QualType if it refers to the 'this'.
  QualType getType() const {
    if (FDs.empty()) {
      if (const auto *VD = Var.dyn_cast<const VarDecl *>())
        return VD->getType();
      if (const auto *MT = Var.dyn_cast<const MaterializeTemporaryExpr *>())
        return MT->getType();
      return {}; // Refers to 'this' pointer.
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

  bool isLifetimeExtendedTemporaryBy(const ValueDecl *VD) const {
    return isLifetimeExtendedTemporary() &&
           Var.get<const MaterializeTemporaryExpr *>()->getExtendingDecl() ==
               VD;
  }

  // Is the pset of this Variable allowed to contain null?
  bool mightBeNull() const {
    if (isThisPointer())
      return false;
    return isNullableType(getType());
  }

  bool isCategoryPointer() const {
    if (isThisPointer())
      return true;
    return classifyTypeCategory(getType()) == TypeCategory::Pointer;
  }

  const VarDecl *asVarDecl() const { return Var.dyn_cast<const VarDecl *>(); }

  // Chain of field accesses starting from VD. Types must match.
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

  /// Possibly empty list of fields on Var first entry is the field on VD,
  /// next entry is the field inside there, etc.
  llvm::SmallVector<const FieldDecl *, 8> FDs;
};

/// The reason why a pset became invalid
/// Invariant: (Reason != POINTEE_LEFT_SCOPE || Pointee) && Loc.isValid()
// TODO: We should use source ranges rather than single locations
//       for user friendliness.
class InvalidationReason {
  enum EReason {
    NOT_INITIALIZED,
    POINTEE_LEFT_SCOPE,
    TEMPORARY_LEFT_SCOPE,
    POINTER_ARITHMETIC,
    FORBIDDEN_CAST,
    DEREFERENCED,
    MODIFIED
  } Reason;

  const VarDecl *Pointee;
  SourceLocation Loc;

  InvalidationReason(SourceLocation Loc, EReason Reason,
                     const VarDecl *Pointee = nullptr)
      : Reason(Reason), Pointee(Pointee), Loc(Loc) {
    assert(Loc.isValid());
  }

public:
  SourceLocation getLoc() const { return Loc; }

  void emitNote(LifetimeReporterBase &Reporter) const {
    switch (Reason) {
    case NOT_INITIALIZED:
      Reporter.noteNeverInitialized(Loc);
      return;
    case POINTEE_LEFT_SCOPE:
      assert(Pointee);
      Reporter.notePointeeLeftScope(Loc, Pointee->getNameAsString());
      return;
    case TEMPORARY_LEFT_SCOPE:
      Reporter.noteTemporaryDestroyed(Loc);
      return;
    case FORBIDDEN_CAST: // TODO: add own diagnostic
      Reporter.noteForbiddenCast(Loc);
      return;
    case POINTER_ARITHMETIC:
      Reporter.notePointerArithmetic(Loc);
      return;
    case DEREFERENCED:
      Reporter.noteDereferenced(Loc);
      return;
    case MODIFIED:
      Reporter.noteModified(Loc);
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

  static InvalidationReason ForbiddenCast(SourceLocation Loc) {
    return {Loc, FORBIDDEN_CAST};
  }

  static InvalidationReason Modified(SourceLocation Loc) {
    return {Loc, MODIFIED};
  }
};

/// The reason how null entered a pset.
class NullReason {
  SourceLocation Loc;

public:
  NullReason(SourceLocation Loc) : Loc(Loc) { assert(Loc.isValid()); }
  void emitNote(LifetimeReporterBase &Reporter) const {
    Reporter.noteAssigned(Loc);
  }
};

/// A pset (points-to set) can contain:
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

  void explainWhyInvalid(LifetimeReporterBase &Reporter) const {
    for (auto &R : InvReasons)
      R.emitNote(Reporter);
  }

  void explainWhyNull(LifetimeReporterBase &Reporter) const {
    for (auto &R : NullReasons)
      R.emitNote(Reporter);
  }

  bool containsInvalid() const { return ContainsInvalid; }
  bool isInvalid() const {
    return !ContainsNull && !ContainsStatic && ContainsInvalid && Vars.empty();
  }

  bool isUnknown() const {
    return !ContainsInvalid && !ContainsNull && !ContainsStatic && Vars.empty();
  }

  /// Returns true if this pset contains Variable with the same or a lower order
  /// i.e. whether invalidating (Variable, order) would invalidate this pset.
  bool contains(Variable Var, unsigned Order = 0) const {
    auto I = Vars.find(Var);
    return I != Vars.end() && I->second >= Order;
  }

  /// Returns true if we look for S and we have S.field in the set.
  bool containsBase(Variable Var, unsigned Order = 0) const {
    auto I = llvm::find_if(
        Vars, [Var, Order](const std::pair<Variable, unsigned> &Other) {
          return Var.isBaseEqual(Other.first) && Order <= Other.second;
        });
    return I != Vars.end() && I->second >= Order;
  }

  bool containsNull() const { return ContainsNull; }
  bool isNull() const {
    return ContainsNull && !ContainsStatic && !ContainsInvalid && Vars.empty();
  }
  void removeNull() { ContainsNull = false; }

  bool containsStatic() const { return ContainsStatic; }
  bool isStatic() const {
    return ContainsStatic && !ContainsNull && !ContainsInvalid && Vars.empty();
  }
  void addStatic() { ContainsStatic = true; }

  bool isSingleton() const {
    return !ContainsInvalid &&
           (ContainsStatic ^ ContainsNull ^ (Vars.size() == 1));
  }

  const std::map<Variable, unsigned> &vars() const { return Vars; }

  const std::vector<InvalidationReason> &invReasons() const {
    return InvReasons;
  }
  const std::vector<NullReason> &nullReasons() const { return NullReasons; }

  bool isSubstitutableFor(const PSet &O) {
    // If 'this' includes invalid, then 'O' must include invalid.
    if (ContainsInvalid && !O.ContainsInvalid)
      return false;

    // If 'this' includes null, then 'O' must include null.
    if (ContainsNull && !O.ContainsNull)
      return false;

    // If 'O' includes static and no x or o, then 'this' must include static and
    // no x or o.
    if (!ContainsStatic && O.ContainsStatic)
      return false;

    // If 'this' includes o'', then 'O' must include o'' or o'. (etc.)
    for (auto &kv : Vars) {
      auto &V = kv.first;
      auto Order = kv.second;
      auto i = O.Vars.find(V);
      if (i == O.Vars.end() || i->second > Order)
        return false;
    }

    // TODO
    // If 'this' includes o'', then 'O' must include o'' or o'. (etc.)
    // If 'this' includes o', then 'O' must include o'.
    // If 'this' includes o, then 'O' must include o or o' or o'' or some x with
    // lifetime less than o. If 'O' includes one or more xb1..n, where
    // xbshortest is the one with shortest lifetime, then a must include static
    // or xa1..m where all have longer lifetime than xbshortest.
    return true;
  }

  std::string str() const {
    if (isUnknown())
      return "((unknown))";
    SmallVector<std::string, 16> Entries;
    if (ContainsInvalid)
      Entries.push_back("(invalid)");
    if (ContainsNull)
      Entries.push_back("(null)");
    if (ContainsStatic)
      Entries.push_back("(static)");
    for (const auto &V : Vars) {
      Entries.push_back(V.first.getName());
      for (size_t j = 0; j < V.second; ++j)
        Entries.back().append("'");
    }
    return "(" + llvm::join(Entries, ", ") + ")";
  }

  void print(raw_ostream &Out) const { Out << str() << "\n"; }

  /// Merge contents of other pset into this.
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

    for (const auto &VO : O.Vars) {
      auto V = Vars.find(VO.first);
      if (V == Vars.end()) {
        Vars.insert(VO);
      } else {
        // If this would contain o' and o'' it would be invalidated on KILL(o')
        // and KILL(o'') which is the same for a pset only containing o''.
        V->second = std::max(V->second, VO.second);
      }
    }
  }

  PSet operator+(const PSet &O) const {
    PSet Ret = *this;
    Ret.merge(O);
    return Ret;
  }

  void insert(Variable Var, unsigned Order = 0) {
    if (Var.hasGlobalStorage()) {
      ContainsStatic = true;
      return;
    }

    // If this would contain o' and o'' it would be invalidated on KILL(o')
    // and KILL(o'') which is the same for a pset only containing o''.
    if (Vars.count(Var))
      Order = std::max(Vars[Var], Order);

    Vars[Var] = Order;
  }

  void addFieldRef(const FieldDecl *FD) {
    std::map<Variable, unsigned> NewVars;
    for (auto &VO : Vars) {
      Variable Var = VO.first;
      Var.addFieldRef(FD);
      NewVars.insert(std::make_pair(Var, VO.second));
    }
    Vars = NewVars;
  }

  void appendNullReason(NullReason R) { NullReasons.push_back(R); }

  void appendInvalidReason(InvalidationReason R) { InvReasons.push_back(R); }

  /// The pointer is dangling
  static PSet invalid(InvalidationReason Reason) {
    return invalid(std::vector<InvalidationReason>{Reason});
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

  /// A pset that contains (static), (null)
  static PSet staticVar(bool Nullable) {
    PSet ret;
    ret.ContainsNull = Nullable;
    ret.ContainsStatic = true;
    return ret;
  }

  /// The pset contains one of obj, obj' or obj''
  static PSet singleton(Variable Var, bool Nullable = false,
                        unsigned order = 0) {
    PSet ret;
    if (Var.hasGlobalStorage())
      ret.ContainsStatic = true;
    else
      ret.Vars.emplace(Var, order);
    ret.ContainsNull = Nullable;
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

} // namespace lifetime
} // namespace clang

#endif // LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMEPSET_H
