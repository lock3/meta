
//=- LifetimeAttrHandling.cpp - Diagnose lifetime violations -*- C++ -*-======//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/Attr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/Analysis/Analyses/Lifetime.h"
#include "clang/Analysis/Analyses/LifetimePsetBuilder.h"

namespace clang {
namespace lifetime {

// Easier access the attribute's representation.
using AttrPointsToMap = LifetimeContractAttr::PointsToMap;

static const Expr *ignoreReturnValues(const Expr *E) {
  const Expr *Original;
  do {
    Original = E;
    E = E->IgnoreImplicit();
    if (const auto *CE = dyn_cast<CXXConstructExpr>(E))
      E = CE->getArg(0);
    if (const auto *MCE = dyn_cast<CXXMemberCallExpr>(E)) {
      const auto *CD =
          dyn_cast_or_null<CXXConversionDecl>(MCE->getDirectCallee());
      if (CD)
        E = MCE->getImplicitObjectArgument();
    }
  } while (E != Original);
  return E;
}

static const ParmVarDecl *toCanonicalParmVar(const ParmVarDecl *PVD) {
  const auto *FD = dyn_cast<FunctionDecl>(PVD->getDeclContext());
  return FD->getCanonicalDecl()->getParamDecl(PVD->getFunctionScopeIndex());
}

// This function can either collect the PSets of the symbols based on a lookup
// table or just the symbols into a pset if the lookup table is nullptr.
static ContractPSet collectPSet(const Expr *E, const AttrPointsToMap *Lookup,
                                SourceRange *FailRange) {
  ContractPSet Result;
  if (const auto *DRE = dyn_cast<DeclRefExpr>(E)) {
    const auto *VD = dyn_cast<VarDecl>(DRE->getDecl());
    if (!VD) {
      *FailRange = DRE->getSourceRange();
      return Result;
    }
    StringRef Name = VD->getName();
    if (Name == "Null") {
      Result.ContainsNull = true;
      return Result;
    } else if (Name == "Static") {
      Result.ContainsStatic = true;
      return Result;
    } else if (Name == "Invalid") {
      Result.ContainsInvalid = true;
      return Result;
    } else if (Name == "Return") {
      Result.Vars.insert(ContractVariable::returnVal());
      return Result;
    } else {
      const auto *PVD = dyn_cast<ParmVarDecl>(VD);
      if (!PVD) {
        *FailRange = DRE->getSourceRange();
        return Result;
      }
      if (Lookup) {
        auto It = Lookup->find(toCanonicalParmVar(PVD));
        assert(It != Lookup->end());
        return It->second;
      } else {
        Result.Vars.insert(toCanonicalParmVar(PVD));
        return Result;
      }
    }
    *FailRange = DRE->getSourceRange();
    return Result;
  } else if (const auto *StdInit = dyn_cast<CXXStdInitializerListExpr>(E)) {
    E = StdInit->getSubExpr()->IgnoreImplicit();
    if (const auto *InitList = dyn_cast<InitListExpr>(E)) {
      for (const auto *Init : InitList->inits()) {
        ContractPSet Elem =
            collectPSet(ignoreReturnValues(Init), Lookup, FailRange);
        if (Elem.isEmpty())
          return Elem;
        Result.merge(Elem);
      }
    }
    return Result;
  } else if (const auto *CE = dyn_cast<CallExpr>(E)) {
    const FunctionDecl *FD = CE->getDirectCallee();
    if (!FD || !FD->getIdentifier() || FD->getName() != "deref") {
      *FailRange = CE->getSourceRange();
      return Result;
    }
    Result = collectPSet(ignoreReturnValues(CE->getArg(0)), Lookup, FailRange);
    auto VarsCopy = Result.Vars;
    Result.Vars.clear();
    for (auto Var : VarsCopy)
      Result.Vars.insert(Var.deref());
    return Result;
  }
  *FailRange = E->getSourceRange();
  return Result;
}

// This function and the callees are have the sole purpose of matching the
// AST that describes the contracts. We are only interested in identifier names
// of function calls and variables. The AST, however, has a lot of other
// information such as casts, termporary objects and so on. They do not have
// any semantic meaning for contracts so much of the code is just skipping
// these unwanted nodes. The rest is collecting the identifiers and their
// hierarchy. This code is subject to change as the language defining the
// contracts is changing.
// Also, the code might be rewritten a more simple way in the future
// piggybacking this work: https://reviews.llvm.org/rL365355
//
// When we have a post condition like:
//     pset(Return) == pset(a)
// We need to look up the Pset of 'a' in preconditions but we need to
// record the postcondition in the postconditions. This is why this
// function takes two AttrPointsToMaps.
static SourceRange fillPointersFromExpr(const Expr *E, AttrPointsToMap &Fill,
                                        const AttrPointsToMap &Lookup) {
  const auto *CE = dyn_cast<CallExpr>(E);
  if (!CE)
    return E->getSourceRange();
  const FunctionDecl *FD = CE->getDirectCallee();
  if (!FD || !FD->getIdentifier() || FD->getName() != "lifetime")
    return E->getSourceRange();

  const Expr *LHS = ignoreReturnValues(CE->getArg(0));
  if (!LHS)
    return CE->getArg(0)->getSourceRange();
  const Expr *RHS = ignoreReturnValues(CE->getArg(1));
  if (!RHS)
    return CE->getArg(1)->getSourceRange();

  SourceRange ErrorRange;
  ContractPSet LhsPSet = collectPSet(LHS, nullptr, &ErrorRange);
  if (LhsPSet.Vars.size() != 1)
    return LHS->getSourceRange();
  if (ErrorRange.isValid())
    return ErrorRange;

  ContractVariable VD = *LhsPSet.Vars.begin();
  ContractPSet RhsPSet = collectPSet(RHS, &Lookup, &ErrorRange);
  if (ErrorRange.isValid())
    return ErrorRange;
  Fill[VD] = RhsPSet;
  return SourceRange();
}

/// Interpret the expression argument of gsl::lifetime_in/out attributes
/// to create the ContractVariable representation.
static SourceRange
fillIOVarsFromExpr(const Expr *E,
                   llvm::SmallVectorImpl<ContractVariable> &ToFill) {
  SourceRange ErrorRange;
  // TODO: This is not restrictive enought. E.g. we should reject
  // lifetime_out(Null).
  ContractPSet PS = collectPSet(E, nullptr, &ErrorRange);
  if (PS.Vars.size() != 1)
    return E->getSourceRange();
  if (ErrorRange.isValid())
    return ErrorRange;

  ToFill.push_back(*PS.Vars.begin());
  return SourceRange();
}

namespace {
class PSetCollector {
public:
  PSetCollector(const FunctionDecl *FD, const ASTContext &ASTCtxt,
                IsConvertibleTy isConvertible, LifetimeReporterBase &Reporter)
      : FD(FD->getCanonicalDecl()), ASTCtxt(ASTCtxt),
        isConvertible(isConvertible), Reporter(Reporter) {}

  /// Fill the default annotations for a function and interpret the
  /// user-provided expressions to create the ContractVariable based
  /// representation of lifetime contracts. Both the expressions and the
  /// ContractVariables are stored in the annotation. The former is only read to
  /// build the latter.
  void fillPSetsForDecl(LifetimeContractAttr *ContractAttr) const {
    // Fill the lifetime_in/lifetime_out annotations.
    ParamDerivedLocations Locations;
    auto *IOAttr = FD->getAttr<LifetimeIOAttr>();
    if (IOAttr && IOAttr->InVars.empty() && IOAttr->OutVars.empty()) {
      for (const Expr *E : IOAttr->InLocExprs) {
        SourceRange Range = fillIOVarsFromExpr(E, IOAttr->InVars);
        if (Range.isValid())
          Reporter.warnUnsupportedExpr(Range);
      }
      for (const Expr *E : IOAttr->OutLocExprs) {
        SourceRange Range = fillIOVarsFromExpr(E, IOAttr->OutVars);
        if (Range.isValid())
          Reporter.warnUnsupportedExpr(Range);
      }
      for (ContractVariable Var : IOAttr->InVars)
        Locations.Input.push_back(Var);
      for (ContractVariable Var : IOAttr->OutVars)
        Locations.Output.push_back(Var);
    }

    // Fill default preconditions and collect data for
    // computing default postconditions.
    for (const ParmVarDecl *PVD : FD->parameters()) {
      QualType ParamType = PVD->getType();
      TypeCategory TC = classifyTypeCategory(ParamType);
      if (TC != TypeCategory::Pointer && TC != TypeCategory::Owner)
        continue;

      ContractVariable ParamLoc(PVD);
      ContractVariable ParamDerefLoc(PVD, 1);
      // Nullable owners are a future note in the paper.
      ContractAttr->PrePSets.emplace(
          ParamLoc, ContractPSet{{ParamDerefLoc}, isNullableType(ParamType)});
      if (TC != TypeCategory::Pointer)
        continue;

      QualType PointeeType = getPointeeType(ParamType);
      ContractPSet ParamDerefPSet{{ContractVariable{PVD, 2}},
                                  isNullableType(PointeeType)};
      switch (classifyTypeCategory(PointeeType)) {
      case TypeCategory::Owner: {
        ContractAttr->PrePSets.emplace(ParamDerefLoc, ParamDerefPSet);
        if (ParamType->isLValueReferenceType()) {
          if (PointeeType.isConstQualified()) {
            addUnannotated(Locations.Input_weak, IOAttr, ParamLoc);
            addUnannotated(Locations.Input_weak, IOAttr, ParamDerefLoc);
          } else {
            addUnannotated(Locations.Input, IOAttr, ParamLoc);
            addUnannotated(Locations.Input, IOAttr, ParamDerefLoc);
          }
        }
        break;
      }
      case TypeCategory::Pointer:
        if (!isLifetimeConst(FD, PointeeType, PVD->getFunctionScopeIndex()) &&
            !isInputAnnotated(IOAttr, ParamDerefLoc)) {
          // Output params are initially invalid.
          ContractPSet InvalidPS;
          InvalidPS.ContainsInvalid = true;
          ContractAttr->PrePSets.emplace(ParamDerefLoc, InvalidPS);
          addUnannotated(Locations.Output, IOAttr, ParamDerefLoc);
        } else {
          ContractAttr->PrePSets.emplace(ParamDerefLoc, ParamDerefPSet);
          // TODO: In the paper we only add derefs for references and not for
          // other pointers. Is this intentional?
          if (ParamType->isLValueReferenceType())
            addUnannotated(Locations.Input, IOAttr, ParamDerefLoc);
        }
        LLVM_FALLTHROUGH;
      default:
        if (!ParamType->isRValueReferenceType())
          addUnannotated(Locations.Input, IOAttr, ParamLoc);
        break;
      }
    }
    // This points to deref this and this considered as input.
    if (const auto *MD = dyn_cast<CXXMethodDecl>(FD)) {
      if (MD->isInstance()) {
        const auto *RD = dyn_cast<CXXRecordDecl>(MD->getParent());
        ContractVariable DerefThis = ContractVariable(RD).deref();
        ContractPSet ThisPSet({DerefThis});
        ContractAttr->PrePSets.emplace(ContractVariable(RD), ThisPSet);
        addUnannotated(Locations.Input, IOAttr, ContractVariable(RD));
        QualType ClassTy = MD->getThisType()->getPointeeType();
        TypeCategory TC = classifyTypeCategory(ClassTy);
        if (TC == TypeCategory::Pointer || TC == TypeCategory::Owner) {
          ContractPSet DerefThisPSet({ContractVariable(RD).deref(2)});
          auto OO = MD->getOverloadedOperator();
          if (OO != OverloadedOperatorKind::OO_Star &&
              OO != OverloadedOperatorKind::OO_Arrow &&
              OO != OverloadedOperatorKind::OO_ArrowStar &&
              OO != OverloadedOperatorKind::OO_Subscript)
            DerefThisPSet.ContainsNull = isNullableType(ClassTy);
          if (const auto *Conv = dyn_cast<CXXConversionDecl>(MD))
            DerefThisPSet.ContainsNull |=
                Conv->getConversionType()->isBooleanType();
          ContractAttr->PrePSets.emplace(DerefThis, DerefThisPSet);
          addUnannotated(Locations.Input, IOAttr, DerefThis);
        }
      }
    }

    // Adust preconditions based on annotations.
    for (const Expr *E : ContractAttr->PreExprs) {
      SourceRange Range = fillPointersFromExpr(E, ContractAttr->PrePSets,
                                               ContractAttr->PrePSets);
      if (Range.isValid())
        Reporter.warnUnsupportedExpr(Range);
    }

    // Compute default postconditions.
    auto computeOutput = [&](QualType OutputType) {
      ContractPSet Ret;
      for (ContractVariable CV : Locations.Input) {
        if (canAssign(getLocationType(CV), OutputType))
          Ret.merge(ContractAttr->PrePSets.at(CV));
      }
      if (Ret.isEmpty()) {
        for (ContractVariable CV : Locations.Input_weak) {
          if (canAssign(getLocationType(CV), OutputType))
            Ret.merge(ContractAttr->PrePSets.at(CV));
        }
      }
      if (Ret.isEmpty())
        Ret.ContainsStatic = true;
      // For not_null types are never null regardless of type matching.
      Ret.ContainsNull = isNullableType(OutputType);
      return Ret;
    };

    if (classifyTypeCategory(FD->getReturnType()) == TypeCategory::Pointer)
      Locations.Output.push_back(ContractVariable::returnVal());

    for (ContractVariable CV : Locations.Output)
      ContractAttr->PostPSets[CV] = computeOutput(getLocationType(CV));

    // Process user defined postconditions.
    for (const Expr *E : ContractAttr->PostExprs) {
      SourceRange Range = fillPointersFromExpr(E, ContractAttr->PostPSets,
                                               ContractAttr->PrePSets);
      if (Range.isValid())
        Reporter.warnUnsupportedExpr(Range);
    }
  }

private:
  bool canAssign(QualType From, QualType To) const {
    QualType FromPointee = getPointeeType(From);
    if (FromPointee.isNull())
      return false;

    QualType ToPointee = getPointeeType(To);
    if (ToPointee.isNull())
      return false;

    return isConvertible(ASTCtxt.getPointerType(FromPointee),
                         ASTCtxt.getPointerType(ToPointee));
  }

  QualType getLocationType(ContractVariable CV) const {
    if (CV == ContractVariable::returnVal())
      return FD->getReturnType();
    return Variable(CV, FD).getType();
  }

  struct ParamDerivedLocations {
    std::vector<ContractVariable> Input_weak;
    std::vector<ContractVariable> Input;
    std::vector<ContractVariable> Output;
  };

  static bool isInputAnnotated(const LifetimeIOAttr *Attr,
                               ContractVariable Var) {
    return Attr && llvm::is_contained(Attr->InVars, Var);
  }

  static void addUnannotated(std::vector<ContractVariable> &To,
                             const LifetimeIOAttr *Attr, ContractVariable Var) {
    if (Attr && (llvm::is_contained(Attr->InVars, Var) ||
                 llvm::is_contained(Attr->OutVars, Var)))
      return;

    To.push_back(Var);
  }

  const FunctionDecl *FD;
  const ASTContext &ASTCtxt;
  IsConvertibleTy isConvertible;
  LifetimeReporterBase &Reporter;
};

} // anonymous namespace

void getLifetimeContracts(PSetsMap &PMap, const FunctionDecl *FD,
                          const ASTContext &ASTCtxt, const CFGBlock *Block,
                          IsConvertibleTy isConvertible,
                          LifetimeReporterBase &Reporter, bool Pre) {
  auto *ContractAttr = FD->getCanonicalDecl()->getAttr<LifetimeContractAttr>();
  if (!ContractAttr)
    return;

  // TODO: this check is insufficient for functions like int f(int);
  if (ContractAttr->PrePSets.empty() && ContractAttr->PostPSets.empty()) {
    PSetCollector Collector(FD, ASTCtxt, isConvertible, Reporter);
    Collector.fillPSetsForDecl(ContractAttr);
  }

  if (Pre) {
    for (const auto &Pair : ContractAttr->PrePSets) {
      Variable V(Pair.first, FD);
      PSet PS(Pair.second, FD);
      if (const auto *PVD = dyn_cast_or_null<ParmVarDecl>(V.asVarDecl())) {
        if (!V.isField() && !V.isDeref() && PS.containsNull())
          PS.addNullReason(
              NullReason::parameterNull(PVD->getSourceRange(), Block));
        if (PS.containsInvalid())
          PS = PSet::invalid(InvalidationReason::NotInitialized(
              PVD->getSourceRange(), Block));
      }
      PMap.emplace(V, PS);
    }
  } else {
    for (const auto &Pair : ContractAttr->PostPSets)
      PMap.emplace(Variable(Pair.first, FD), PSet(Pair.second, FD));
  }
}

} // namespace lifetime
} // namespace clang
