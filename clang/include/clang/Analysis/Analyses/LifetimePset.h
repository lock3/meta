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
      return FDs.size() < O.FDs.size();

    for (auto I = FDs.begin(), J = O.FDs.begin(); I != FDs.end(); ++I, ++J) {
      if (*I != *J)
        return std::less<const FieldDecl *>()(*I, *J);
    }
    return false;
  }

  bool isBaseEqual(const Variable &O) const { return Var == O.Var; }

  bool hasStaticLifetime() const {
    if (const auto *VD = Var.dyn_cast<const VarDecl *>())
      return VD->hasGlobalStorage();
    return isThisPointer() && !FDs.empty();
  }

  /// Returns QualType of Variable or empty QualType if it refers to the 'this'.
  /// TODO: Should we cache the type instead of calculating?
  QualType getType() const {
    QualType Base;
    if (const auto *VD = Var.dyn_cast<const VarDecl *>())
      Base = VD->getType();
    else if (const auto *MT = Var.dyn_cast<const MaterializeTemporaryExpr *>())
      Base = MT->getType();
    else
      assert(!FDs.empty() && "Not yet supported for temporary and this.");

    for (auto It = FDs.rbegin(); It != FDs.rend(); ++It) {
      if (*It) {
        assert(isThisPointer() || isTemporary() ||
               (*It)->getParent() == Base->getAsCXXRecordDecl() ||
               Base->getAsCXXRecordDecl()->isDerivedFrom(
                   dyn_cast<CXXRecordDecl>((*It)->getParent())));
        Base = (*It)->getType();
        break;
      } else {
        Base = getPointeeType(Base);
      }
    }
    return Base;
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

  void deref() { FDs.push_back(nullptr); }

  bool isDeref() const { return !FDs.empty() && FDs.front() == nullptr; }

  std::string getName() const {
    std::string Ret;
    if (Var.is<const MaterializeTemporaryExpr *>()) {
      auto *MD = Var.get<const MaterializeTemporaryExpr *>();
      if (MD) {
        Ret = "(lifetime-extended temporary through ";
        if (MD->getExtendingDecl())
          Ret += std::string(MD->getExtendingDecl()->getName()) + ")";
        else
          Ret += "(unknown))";
      } else {
        Ret = "(temporary)";
      }
    } else {
      auto *VD = Var.get<const VarDecl *>();
      Ret = (VD ? std::string(VD->getName()) : "this");
    }

    for (const auto *FD : FDs) {
      if (FD)
        Ret += "." + std::string(FD->getName());
      else
        Ret = "(*" + Ret + ")";
    }
    return Ret;
  }

  llvm::PointerUnion<const VarDecl *, const MaterializeTemporaryExpr *> Var;

  /// Possibly empty list of fields and deref operations on the base.
  /// The First entry is the field on base, next entry is the field inside
  /// there, etc. Null pointers represent a deref operation.
  llvm::SmallVector<const FieldDecl *, 8> FDs;
};

/// The reason why a pset became invalid
/// Invariant: (Reason != POINTEE_LEFT_SCOPE || Pointee) && Range.isValid()
// TODO: We should use source ranges rather than single locations
//       for user friendliness.
class InvalidationReason {
  enum EReason {
    NOT_INITIALIZED,
    POINTEE_LEFT_SCOPE,
    TEMPORARY_LEFT_SCOPE,
    FORBIDDEN_CAST,
    DEREFERENCED,
    MODIFIED,
    DELETED
  } Reason;

  const VarDecl *Pointee;
  SourceRange Range;

  InvalidationReason(SourceRange Range, EReason Reason,
                     const VarDecl *Pointee = nullptr)
      : Reason(Reason), Pointee(Pointee), Range(Range) {
    assert(Range.isValid());
  }

public:
  SourceLocation getLoc() const { return Range.getBegin(); }

  void emitNote(LifetimeReporterBase &Reporter) const {
    switch (Reason) {
    case NOT_INITIALIZED:
      Reporter.noteNeverInitialized(getLoc());
      return;
    case POINTEE_LEFT_SCOPE:
      assert(Pointee);
      Reporter.notePointeeLeftScope(getLoc(), Pointee->getNameAsString());
      return;
    case TEMPORARY_LEFT_SCOPE:
      Reporter.noteTemporaryDestroyed(getLoc());
      return;
    case FORBIDDEN_CAST:
      Reporter.noteForbiddenCast(getLoc());
      return;
    case DEREFERENCED:
      Reporter.noteDereferenced(getLoc());
      return;
    case MODIFIED:
      Reporter.noteModified(getLoc());
      return;
    case DELETED:
      Reporter.noteDeleted(getLoc());
      return;
    }
    llvm_unreachable("Invalid InvalidationReason::Reason");
  }

  static InvalidationReason NotInitialized(SourceRange Range) {
    return {Range, NOT_INITIALIZED};
  }

  static InvalidationReason PointeeLeftScope(SourceRange Range,
                                             const VarDecl *Pointee) {
    assert(Pointee);
    return {Range, POINTEE_LEFT_SCOPE, Pointee};
  }

  static InvalidationReason TemporaryLeftScope(SourceRange Range) {
    return {Range, TEMPORARY_LEFT_SCOPE};
  }

  static InvalidationReason Dereferenced(SourceRange Range) {
    return {Range, DEREFERENCED};
  }

  static InvalidationReason ForbiddenCast(SourceRange Range) {
    return {Range, FORBIDDEN_CAST};
  }

  static InvalidationReason Modified(SourceRange Range) {
    return {Range, MODIFIED};
  }

  static InvalidationReason Deleted(SourceRange Range) {
    return {Range, DELETED};
  }
};

/// The reason how null entered a pset.
class NullReason {
  SourceRange Range;

public:
  enum EReason {
    ASSIGNED,
    PARAMETER_NULL,
    DEFAULT_CONSTRUCTED,
    COMPARED_TO_NULL,
    NULLPTR_CONSTANT
  } Reason;

  NullReason(SourceRange Range, EReason Reason) : Range(Range), Reason(Reason) {
    assert(Range.isValid());
  }

  static NullReason assigned(SourceRange Range) { return {Range, ASSIGNED}; }

  static NullReason parameterNull(SourceRange Range) {
    return {Range, PARAMETER_NULL};
  }

  static NullReason defaultConstructed(SourceRange Range) {
    return {Range, DEFAULT_CONSTRUCTED};
  }

  static NullReason comparedToNull(SourceRange Range) {
    return {Range, COMPARED_TO_NULL};
  }

  static NullReason nullptrConstant(SourceRange Range) {
    return {Range, NULLPTR_CONSTANT};
  }

  void emitNote(LifetimeReporterBase &Reporter) const {
    switch (Reason) {
    case ASSIGNED:
      Reporter.noteAssigned(Range.getBegin());
      break;
    case PARAMETER_NULL:
      Reporter.noteParameterNull(Range.getBegin());
      break;
    case DEFAULT_CONSTRUCTED:
      Reporter.noteNullDefaultConstructed(Range.getBegin());
      break;
    case COMPARED_TO_NULL:
      Reporter.noteNullComparedToNull(Range.getBegin());
      break;
    case NULLPTR_CONSTANT:
      break; // not diagnosed, hopefully obvious
    }
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
  void addNull(NullReason Reason) {
    if (ContainsNull)
      return;
    ContainsNull = true;
    NullReasons.push_back(Reason);
  }
  void removeNull() { ContainsNull = false; }
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
    if (Var.hasStaticLifetime()) {
      ContainsStatic = true;
      return;
    }

    // If this would contain o' and o'' it would be invalidated on KILL(o')
    // and KILL(o'') which is the same for a pset only containing o''.
    auto It = Vars.find(Var);
    if (It != Vars.end())
      Order = std::max(It->second, Order);

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

  /// The pset contains one of obj, obj' or obj''
  static PSet singleton(Variable Var, unsigned order = 0) {
    PSet ret;
    if (Var.hasStaticLifetime())
      ret.ContainsStatic = true;
    else
      ret.Vars.emplace(Var, order);
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

using PSetsMap = std::map<Variable, PSet>;

} // namespace lifetime
} // namespace clang

#endif // LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMEPSET_H
