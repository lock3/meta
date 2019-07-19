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

#include "clang/AST/Decl.h"
#include "clang/AST/ExprCXX.h"
#include "clang/Analysis/Analyses/LifetimeTypeCategory.h"
#include <map>
#include <set>
#include <sstream>
#include <vector>

namespace clang {
namespace lifetime {

/// A Variable can represent a base:
/// - a local variable: Var contains a non-null VarDecl
/// - the this pointer: Var contains a null VarDecl
/// - a life-time extended temporary: Var contains a non-null
/// MaterializeTemporaryExpr
/// - a normal temporary: Var contains a null MaterializeTemporaryExpr
/// plus fields of them (in member FDs).
/// And a list of dereference and field select operations that applied
/// consecutively to the base.
struct Variable {
  Variable(const VarDecl *VD, llvm::ArrayRef<const FieldDecl *> FDs = {})
      : Var(VD), FDs(FDs.begin(), FDs.end()) {}
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
      return FDs.size() < O.FDs.size();

    for (auto I = FDs.begin(), J = O.FDs.begin(); I != FDs.end(); ++I, ++J) {
      if (*I != *J)
        return std::less<const FieldDecl *>()(*I, *J);
    }
    return false;
  }

  // TODO: is this what we want when derefs are involved?
  bool isBaseEqual(const Variable &O) const { return Var == O.Var; }

  // TODO: is this what we want when derefs are involved?
  bool hasStaticLifetime() const {
    if (const auto *VD = Var.dyn_cast<const VarDecl *>())
      return VD->hasGlobalStorage();
    return isThisPointer() && !FDs.empty();
  }

  /// Returns QualType of Variable or empty QualType if it refers to the 'this'.
  /// TODO: Should we cache the type instead of calculating?
  QualType getType() const {
    int Order;
    return getTypeAndOrder(Order);
  }

  bool isField() const { return !FDs.empty() && FDs.back(); }

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

  const VarDecl *asVarDecl() const { return Var.dyn_cast<const VarDecl *>(); }

  // Chain of field accesses starting from VD. Types must match.
  void addFieldRef(const FieldDecl *FD) { FDs.push_back(FD); }

  Variable &deref(int Num = 1) {
    while (Num--)
      FDs.push_back(nullptr);
    return *this;
  }

  bool isDeref() const { return !FDs.empty() && FDs.front() == nullptr; }

  unsigned getOrder() const {
    int Order;
    getTypeAndOrder(Order);
    return Order >= 0 ? Order : 0;
  }

  std::string getName() const {
    std::string Ret;
    if (Var.is<const MaterializeTemporaryExpr *>()) {
      auto *MD = Var.get<const MaterializeTemporaryExpr *>();
      if (MD && MD->getExtendingDecl()) {
        Ret = "(lifetime-extended temporary through " +
              MD->getExtendingDecl()->getName().str() + ")";
      } else
        Ret = "(temporary)";
    } else {
      auto *VD = Var.get<const VarDecl *>();
      Ret = (VD ? VD->getName() : "this");
    }

    for (const auto *FD : FDs) {
      if (FD)
        Ret += "." + std::string(FD->getName());
      else
        Ret = "(*" + Ret + ")";
    }
    return Ret;
  }

private:
  QualType getTypeAndOrder(int &Order) const {
    Order = -1;
    QualType Base;
    if (const auto *VD = Var.dyn_cast<const VarDecl *>())
      Base = VD->getType();
    else if (const auto *MT = Var.dyn_cast<const MaterializeTemporaryExpr *>())
      Base = MT->getType();
    else if (FDs.empty()) {
      // TODO: not supported for this and temporary yet.
      return Base;
    }

    if (!Base.isNull() && classifyTypeCategory(Base) == TypeCategory::Owner)
      Order = 0;
    for (auto It = FDs.begin(); It != FDs.end(); ++It) {
      if (*It) {
        assert(isThisPointer() || isTemporary() ||
               (*It)->getParent() == Base->getAsCXXRecordDecl() ||
               Base->getAsCXXRecordDecl()->isDerivedFrom(
                   dyn_cast<CXXRecordDecl>((*It)->getParent())));
        Base = (*It)->getType();
        if (Order == -1 && classifyTypeCategory(Base) == TypeCategory::Owner)
          Order = 0;
      } else {
        Base = getPointeeType(Base);
        if (Order >= 0)
          ++Order;
      }
    }
    return Base;
  }

  llvm::PointerUnion<const VarDecl *, const MaterializeTemporaryExpr *> Var;

  /// Possibly empty list of fields and deref operations on the base.
  /// The First entry is the field on base, next entry is the field inside
  /// there, etc. Null pointers represent a deref operation.
  llvm::SmallVector<const FieldDecl *, 4> FDs;
};

/// The reason why a pset became invalid
/// Invariant: (Reason != POINTEE_LEFT_SCOPE || Pointee) && Range.isValid()
class InvalidationReason {
  NoteType Reason;
  const VarDecl *Pointee;
  SourceRange Range;

  InvalidationReason(SourceRange Range, NoteType Reason,
                     const VarDecl *Pointee = nullptr)
      : Reason(Reason), Pointee(Pointee), Range(Range) {
    assert(Range.isValid());
  }

public:
  SourceRange getRange() const { return Range; }

  void emitNote(LifetimeReporterBase &Reporter) const {
    if (Reason == NoteType::PointeeLeftScope) {
      assert(Pointee);
      Reporter.notePointeeLeftScope(Range, Pointee->getNameAsString());
      return;
    }
    Reporter.note(Reason, Range);
  }

  static InvalidationReason NotInitialized(SourceRange Range) {
    return {Range, NoteType::NeverInit};
  }

  static InvalidationReason PointeeLeftScope(SourceRange Range,
                                             const VarDecl *Pointee) {
    assert(Pointee);
    return {Range, NoteType::PointeeLeftScope, Pointee};
  }

  static InvalidationReason TemporaryLeftScope(SourceRange Range) {
    return {Range, NoteType::TempDestroyed};
  }

  static InvalidationReason Dereferenced(SourceRange Range) {
    return {Range, NoteType::Dereferenced};
  }

  static InvalidationReason ForbiddenCast(SourceRange Range) {
    return {Range, NoteType::ForbiddenCast};
  }

  static InvalidationReason Modified(SourceRange Range) {
    return {Range, NoteType::Modified};
  }

  static InvalidationReason Deleted(SourceRange Range) {
    return {Range, NoteType::Deleted};
  }
};

/// The reason how null entered a pset.
class NullReason {
  SourceRange Range;

public:
  NoteType Reason;

  NullReason(SourceRange Range, NoteType Reason)
      : Range(Range), Reason(Reason) {
    assert(Range.isValid());
  }

  static NullReason assigned(SourceRange Range) {
    return {Range, NoteType::Assigned};
  }

  static NullReason parameterNull(SourceRange Range) {
    return {Range, NoteType::ParamNull};
  }

  static NullReason defaultConstructed(SourceRange Range) {
    return {Range, NoteType::NullDefaultConstructed};
  }

  static NullReason comparedToNull(SourceRange Range) {
    return {Range, NoteType::ComparedToNull};
  }

  static NullReason nullptrConstant(SourceRange Range) {
    return {Range, NoteType::NullConstant};
  }

  void emitNote(LifetimeReporterBase &Reporter) const {
    if (Reason == NoteType::NullConstant)
      return; // not diagnosed, hopefully obvious
    Reporter.note(Reason, Range);
  }
};

/// A pset (points-to set) can contain:
/// - null
/// - static
/// - invalid
/// - variables
/// It a Pset contains non of that, its "unknown".
class PSet {
public:
  // Initializes an unknown pset
  PSet() : ContainsNull(false), ContainsInvalid(false), ContainsStatic(false) {}
  PSet(const LifetimeContractAttr::PointsToSet &S,
       ArrayRef<const ParmVarDecl *> Params)
      : ContainsNull(S.HasNull), ContainsInvalid(S.HasInvalid),
        ContainsStatic(S.HasStatic) {
    for (const LifetimeContractAttr::PointsToLoc &L : S.Pointees) {
      assert(L.BaseIndex != LifetimeContractAttr::PointsToLoc::ReturnVal);
      if (L.BaseIndex == LifetimeContractAttr::PointsToLoc::ThisVal)
        Vars.emplace(nullptr, L.FDs);
      else
        Vars.emplace(Params[L.BaseIndex], L.FDs);
    }
  }

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

  /// Returns true if we look for S and we have S.field in the set.
  bool containsBase(Variable Var, unsigned Order = 0) const {
    auto I = llvm::find_if(Vars, [Var, Order](const Variable &Other) {
      return Var.isBaseEqual(Other) && Order <= Other.getOrder();
    });
    return I != Vars.end();
  }

  bool containsNull() const { return ContainsNull; }
  bool isNull() const {
    return ContainsNull && !ContainsStatic && !ContainsInvalid && Vars.empty();
  }
  void addNull(NullReason Reason) {
    if (ContainsNull)
      return;
    ContainsNull = true;
    NullReasons.push_back(Reason);
  }
  void removeNull() {
    ContainsNull = false;
    NullReasons.clear();
  }
  void removeEverythingButNull() {
    ContainsInvalid = false;
    InvReasons.clear();
    ContainsStatic = false;
    Vars.clear();
  }

  void addNullReason(NullReason Reason) {
    assert(ContainsNull);
    NullReasons.push_back(Reason);
  }

  bool containsStatic() const { return ContainsStatic; }
  bool isStatic() const {
    return ContainsStatic && !ContainsNull && !ContainsInvalid && Vars.empty();
  }
  void addStatic() { ContainsStatic = true; }

  bool isSingleton() const {
    return !ContainsInvalid &&
           (ContainsStatic ^ ContainsNull ^ (Vars.size() == 1));
  }

  const std::set<Variable> &vars() const { return Vars; }

  const std::vector<InvalidationReason> &invReasons() const {
    return InvReasons;
  }
  const std::vector<NullReason> &nullReasons() const { return NullReasons; }

  bool checkSubstitutableFor(const PSet &O, SourceRange Range,
                             LifetimeReporterBase &Reporter) {
    // Everything is substitutable for invalid.
    if (O.ContainsInvalid)
      return true;
    
    // If 'this' includes invalid, then 'O' must include invalid.
    if (ContainsInvalid) {
      Reporter.warnParameterDangling(Range, /*Indirectly=*/false);
      explainWhyInvalid(Reporter);
      return false;
    }

    // If 'this' includes null, then 'O' must include null.
    if (ContainsNull && !O.ContainsNull) {
      Reporter.warn(WarnType::ParamNull, Range, !isNull());
      explainWhyNull(Reporter);
      return false;
    }

    // If 'O' includes static and no x or o, then 'this' must include static and
    // no x or o.
    if (!ContainsStatic && O.ContainsStatic)
      return false;

    // If 'this' includes o'', then 'O' must include o'' or o'. (etc.)
    for (auto &V : Vars) {
      auto I = O.Vars.find(V);
      if (I == O.Vars.end() || I->getOrder() > V.getOrder())
        return false;
    }

    // TODO
    // If 'this' includes o'', then 'O' must include o'' or o'. (etc.)
    // If 'this' includes o', then 'O' must include o'.
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
    for (const auto &V : Vars)
      Entries.push_back(V.getName());
    std::sort(Entries.begin(), Entries.end());
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

    Vars.insert(O.Vars.begin(), O.Vars.end());
  }

  // This method is used to actualize the PSet of a contract with the arguments
  // of a call.
  void bind(Variable ToReplace, const PSet &To) {
    // Replace valid deref locations.
    if (Vars.erase(ToReplace))
      Vars.insert(To.Vars.begin(), To.Vars.end());
  }

  PSet operator+(const PSet &O) const {
    PSet Ret = *this;
    Ret.merge(O);
    return Ret;
  }

  void insert(Variable Var, unsigned Deref = 0) {
    if (Var.hasStaticLifetime()) {
      ContainsStatic = true;
      return;
    }

    Vars.insert(Var);
  }

  void addFieldRef(const FieldDecl *FD) {
    std::set<Variable> NewVars;
    for (auto Var : Vars) {
      Var.addFieldRef(FD);
      NewVars.insert(Var);
    }
    Vars = NewVars;
  }

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
  static PSet staticVar(bool Nullable = false) {
    PSet ret;
    ret.ContainsNull = Nullable;
    ret.ContainsStatic = true;
    return ret;
  }

  /// The pset contains one element
  static PSet singleton(Variable Var, unsigned Deref = 0) {
    PSet ret;
    if (Var.hasStaticLifetime())
      ret.ContainsStatic = true;
    else {
      Var.deref(Deref);
      ret.Vars.emplace(Var);
    }
    return ret;
  }

private:
  int ContainsNull : 1;
  int ContainsInvalid : 1;
  int ContainsStatic : 1;
  std::set<Variable> Vars;

  std::vector<InvalidationReason> InvReasons;
  std::vector<NullReason> NullReasons;
}; // namespace lifetime

using PSetsMap = std::map<Variable, PSet>;

} // namespace lifetime
} // namespace clang

#endif // LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMEPSET_H
