
//=- LifetimeAttrHandling.cpp - Diagnose lifetime violations -*- C++ -*-======//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/Analysis/Analyses/LifetimePsetBuilder.h"
#include "clang/AST/ExprCXX.h"

namespace clang {
namespace lifetime {

static const Expr *ignoreReturnValues(const Expr *E) {
  E = E->IgnoreImplicit();
  if (const auto *CE = dyn_cast<CXXConstructExpr>(E))
    return CE->getArg(0)->IgnoreImplicit();
  return E;
}

static const Expr *getGslPsetArg(const Expr *E) {
  E = ignoreReturnValues(E);
  if (const auto *CE = dyn_cast<CallExpr>(E)) {
    const FunctionDecl *FD = CE->getDirectCallee();
    if (!FD || FD->getName() != "pset")
      return nullptr;
    return ignoreReturnValues(CE->getArg(0));
  }
  return nullptr;
}

LifetimeContractAttr::PointsToSet
merge(LifetimeContractAttr::PointsToSet LHS,
      const LifetimeContractAttr::PointsToSet &RHS) {
  LHS.HasNull |= RHS.HasNull;
  LHS.HasStatic |= RHS.HasStatic;
  LHS.HasInvalid |= RHS.HasInvalid;
  for (LifetimeContractAttr::PointsToLoc Loc : RHS.Pointees)
    LHS.Pointees.push_back(Loc);
  return LHS;
}

static LifetimeContractAttr::PointsToSet
collectPSet(const Expr *E, LifetimeContractAttr::PointsToMap &Pointers) {
  LifetimeContractAttr::PointsToSet Result;
  if (const auto *DRE = dyn_cast<DeclRefExpr>(E)) {
    const auto *VD = dyn_cast<VarDecl>(DRE->getDecl());
    if (!VD)
      return Result;
    StringRef Name = VD->getName();
    if (Name == "Null")
      Result.HasNull = true;
    else if (Name == "Static")
      Result.HasStatic = true;
    else if (Name == "Invalid")
      Result.HasInvalid = true;
    else {
      const auto *PVD = dyn_cast<ParmVarDecl>(VD);
      if (!PVD)
        return Result;
      LifetimeContractAttr::PSetKey Key(PVD->getFunctionScopeIndex(), 0);
      Result = merge(Result, Pointers[Key]);
    }
    return Result;
  } else if (const auto *StdInit = dyn_cast<CXXStdInitializerListExpr>(E)) {
    E = StdInit->getSubExpr()->IgnoreImplicit();
    if (const auto *InitList = dyn_cast<InitListExpr>(E)) {
      for (const auto *Init : InitList->inits()) {
        LifetimeContractAttr::PointsToSet Elem =
            collectPSet(ignoreReturnValues(Init), Pointers);
        if (Elem.Pointees.empty())
          return Elem;
        Result = merge(Result, Elem);
      }
    }
  }
  return Result;
}

static bool fillPointersFromExpr(const Expr *E,
                                 LifetimeContractAttr::PointsToMap &Pointers) {
  const auto *OCE = dyn_cast<CXXOperatorCallExpr>(E);
  if (!OCE || OCE->getOperator() != OO_EqualEqual)
    return false;

  const Expr *LHS = getGslPsetArg(OCE->getArg(0));
  if (!LHS)
    return false;
  const Expr *RHS = getGslPsetArg(OCE->getArg(1));
  if (!RHS)
    return false;
  // TODO: setting PSets for deref locs not supported yet.
  //       also allow null/static etc on both sides.
  if (!isa<DeclRefExpr>(LHS))
    std::swap(LHS, RHS);
  if (!isa<DeclRefExpr>(LHS))
    return false;

  LifetimeContractAttr::PSetKey VD(
      cast<ParmVarDecl>(cast<DeclRefExpr>(LHS)->getDecl())
          ->getFunctionScopeIndex(),
      0);
  LifetimeContractAttr::PointsToSet PSet = collectPSet(RHS, Pointers);
  // TODO: diagnose failures for empty PSet.
  Pointers[VD] = PSet;
  return true;
}

static void fillPSetsForDecl(const FunctionDecl *FD,
                             LifetimeContractAttr *PreAttr) {

  LifetimeContractAttr::PointsToMap &Map = PreAttr->PrePSets;
  // Fill default PSets.
  for (const ParmVarDecl *PVD : FD->parameters()) {
    QualType ParamTy = PVD->getType();
    TypeCategory TC = classifyTypeCategory(ParamTy);
    if (TC != TypeCategory::Pointer && TC != TypeCategory::Owner)
      continue;

    LifetimeContractAttr::PSetKey Key(PVD->getFunctionScopeIndex(), 0);
    LifetimeContractAttr::PointsToSet PS;
    LifetimeContractAttr::PointsToLoc ParamDerefLoc;
    ParamDerefLoc.Var = PVD;
    ParamDerefLoc.FDs.push_back(nullptr);
    PS.Pointees.push_back(ParamDerefLoc);
    // TODO: nullable Owners don't exist in the paper (yet?)
    // TODO: nullreason not added yet!
    //       maybe could be added at the Attr->PSet conv?
    if (isNullableType(ParamTy))
      PS.HasNull = true;
    if (TC == TypeCategory::Pointer) {
      LifetimeContractAttr::PSetKey P_deref(PVD->getFunctionScopeIndex(), 1);
      QualType PointeeType = getPointeeType(ParamTy);
      LifetimeContractAttr::PointsToSet DerefPS;
      switch (classifyTypeCategory(PointeeType)) {
      case TypeCategory::Owner: {
        LifetimeContractAttr::PointsToLoc ParamDerefDerefLoc = ParamDerefLoc;
        ParamDerefDerefLoc.FDs.push_back(nullptr);
        DerefPS.Pointees.push_back(ParamDerefDerefLoc);
        if (isNullableType(ParamTy))
          DerefPS.HasNull = true;
        Map.try_emplace(P_deref, DerefPS);
        break;
      }
      case TypeCategory::Pointer:
        if (!PointeeType.isConstQualified()) {
          // Output params are initially invalid.
          DerefPS.HasInvalid = true;
          Map.try_emplace(P_deref, DerefPS);
        } else {
          // staticVar to allow further derefs (if this is Pointer to a
          // Pointer to a Pointer etc)
          DerefPS.HasStatic = true;
          if (isNullableType(ParamTy))
            DerefPS.HasNull = true;
          Map.try_emplace(P_deref, DerefPS);
        }
        LLVM_FALLTHROUGH;
      default:
        break;
      }
    }
    Map.try_emplace(Key, PS);
  }

  // Adust PSets based on annotations.
  for (const Expr *E : PreAttr->PreExprs) {
    if (!fillPointersFromExpr(E, PreAttr->PrePSets))
      continue; // TODO: warn
  }
}

void getPreconditionAssumptions(PSetsMap &PMap, const FunctionDecl *FD) {
  auto *PreAttr = FD->getCanonicalDecl()->getAttr<LifetimeContractAttr>();

  if (PreAttr->PrePSets.empty())
    fillPSetsForDecl(FD, PreAttr);

  for (const auto &Pair : PreAttr->PrePSets) {
    Variable V(FD->getParamDecl(Pair.first.first));
    V.deref(Pair.first.second);
    PSet PS(Pair.second);
    if (const auto *PVD = dyn_cast_or_null<ParmVarDecl>(V.asVarDecl())) {
      if (!V.isField() && !V.isDeref() && PS.containsNull())
        PS.addNullReason(NullReason::parameterNull(PVD->getSourceRange()));
      if (PS.containsInvalid())
        PS = PSet::invalid(
            InvalidationReason::NotInitialized(PVD->getSourceRange()));
    }
    PMap.emplace(V, PS);
  }
  PMap.emplace(Variable::thisPointer(),
               PSet::singleton(Variable::thisPointer()));
}

} // namespace lifetime
} // namespace clang