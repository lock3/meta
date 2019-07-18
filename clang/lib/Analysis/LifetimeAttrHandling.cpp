
//=- LifetimeAttrHandling.cpp - Diagnose lifetime violations -*- C++ -*-======//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/ExprCXX.h"
#include "clang/Analysis/Analyses/Lifetime.h"
#include "clang/Analysis/Analyses/LifetimePsetBuilder.h"

namespace clang {
namespace lifetime {

// Easier access the attribute's representation.
using AttrPointsToSet = LifetimeContractAttr::PointsToSet;
using AttrPointsToMap = LifetimeContractAttr::PointsToMap;
using AttrPointsToLoc = LifetimeContractAttr::PointsToLoc;
using AttrPSetKey = LifetimeContractAttr::PSetKey;

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

static AttrPointsToSet merge(AttrPointsToSet LHS, const AttrPointsToSet &RHS) {
  LHS.HasNull |= RHS.HasNull;
  LHS.HasStatic |= RHS.HasStatic;
  LHS.HasInvalid |= RHS.HasInvalid;
  for (AttrPointsToLoc Loc : RHS.Pointees)
    LHS.Pointees.push_back(Loc);
  return LHS;
}

static bool isEmpty(const AttrPointsToSet Set) {
  return Set.Pointees.empty() && !Set.HasNull && !Set.HasInvalid &&
         Set.HasStatic;
}

static AttrPointsToSet collectPSet(const Expr *E, AttrPointsToMap &Pointers) {
  AttrPointsToSet Result;
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
      AttrPSetKey Key(PVD->getFunctionScopeIndex(), 0);
      Result = merge(Result, Pointers[Key]);
    }
    return Result;
  } else if (const auto *StdInit = dyn_cast<CXXStdInitializerListExpr>(E)) {
    E = StdInit->getSubExpr()->IgnoreImplicit();
    if (const auto *InitList = dyn_cast<InitListExpr>(E)) {
      for (const auto *Init : InitList->inits()) {
        AttrPointsToSet Elem = collectPSet(ignoreReturnValues(Init), Pointers);
        if (Elem.Pointees.empty())
          return Elem;
        Result = merge(Result, Elem);
      }
    }
  }
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
static bool fillPointersFromExpr(const Expr *E, AttrPointsToMap &Pointers) {
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

  AttrPSetKey VD(cast<ParmVarDecl>(cast<DeclRefExpr>(LHS)->getDecl())
                     ->getFunctionScopeIndex(),
                 0);
  AttrPointsToSet PSet = collectPSet(RHS, Pointers);
  // TODO: diagnose failures for empty PSet.
  Pointers[VD] = PSet;
  return true;
}

namespace {
class PSetCollector {
public:
  PSetCollector(const FunctionDecl *FD, const ASTContext &ASTCtxt,
                IsConvertibleTy isConvertible)
      : FD(FD), ASTCtxt(ASTCtxt), isConvertible(isConvertible) {}

  void fillPSetsForDecl(LifetimeContractAttr *ContractAttr) {
    // Fill default PSets.
    // TODO: inputs to outputs matching needs to be done after
    //       user defined annotations are processed.
    ParamDerivedLocations Locations;
    for (const ParmVarDecl *PVD : FD->parameters()) {
      QualType ParamTy = PVD->getType();
      TypeCategory TC = classifyTypeCategory(ParamTy);
      if (TC != TypeCategory::Pointer && TC != TypeCategory::Owner)
        continue;

      AttrPSetKey ParamLoc(PVD->getFunctionScopeIndex(), 0);
      AttrPointsToSet PS;
      AttrPointsToLoc ParamDerefLoc;
      ParamDerefLoc.BaseIndex = ParamLoc.first;
      ParamDerefLoc.FDs.push_back(nullptr);
      PS.Pointees.push_back(ParamDerefLoc);
      // TODO: nullable Owners don't exist in the paper (yet?)
      if (isNullableType(ParamTy))
        PS.HasNull = true;
      if (TC == TypeCategory::Pointer) {
        AttrPSetKey P_deref(PVD->getFunctionScopeIndex(), 1);
        QualType PointeeType = getPointeeType(ParamTy);
        AttrPointsToSet DerefPS;
        AttrPointsToLoc ParamDerefDerefLoc = ParamDerefLoc;
        ParamDerefDerefLoc.FDs.push_back(nullptr);
        DerefPS.Pointees.push_back(ParamDerefDerefLoc);
        if (isNullableType(PointeeType))
          DerefPS.HasNull = true;
        switch (classifyTypeCategory(PointeeType)) {
        case TypeCategory::Owner: {
          ContractAttr->PrePSets.try_emplace(P_deref, DerefPS);
          if (ParamTy->isLValueReferenceType()) {
            if (PointeeType.isConstQualified()) {
              Locations.Input_weak.push_back(ParamLoc);
              Locations.Input_weak.push_back(P_deref);
            } else {
              Locations.Input.push_back(ParamLoc);
              Locations.Input.push_back(P_deref);
            }
          }
          break;
        }
        case TypeCategory::Pointer:
          if (!PointeeType.isConstQualified()) {
            // Output params are initially invalid.
            AttrPointsToSet InvalidPS;
            InvalidPS.HasInvalid = true;
            ContractAttr->PrePSets.try_emplace(P_deref, InvalidPS);
            Locations.Output.push_back(P_deref);
          } else {
            ContractAttr->PrePSets.try_emplace(P_deref, DerefPS);
            // TODO: In the paper we only add derefs for references and not for
            // other pointers. Is this intentional?
            if (ParamTy->isLValueReferenceType())
              Locations.Input.push_back(P_deref);
          }
          LLVM_FALLTHROUGH;
        default:
          if (!ParamTy->isRValueReferenceType())
            Locations.Input.push_back(ParamLoc);
          break;
        }
      }
      ContractAttr->PrePSets.try_emplace(ParamLoc, PS);
    }

    // Compute default outputs
    auto computeOutput = [&](QualType OutputType) {
      AttrPointsToSet Ret;
      for (AttrPSetKey K : Locations.Input) {
        if (canAssign(getLocationType(K), OutputType))
          Ret = merge(Ret, ContractAttr->PrePSets[K]);
      }
      if (isEmpty(Ret)) {
        for (AttrPSetKey K : Locations.Input_weak) {
          if (canAssign(getLocationType(K), OutputType))
            Ret = merge(Ret, ContractAttr->PrePSets[K]);
        }
      }
      // For not_null types assume that the callee did not set them
      // to null.
      if (!isNullableType(OutputType))
        Ret.HasNull = false;
      if (isEmpty(Ret))
        Ret.HasStatic = true;
      return Ret;
    };
    for (AttrPSetKey O : Locations.Output)
      ContractAttr->PostPSets[O] = computeOutput(getLocationType(O));

    // Adust PSets based on annotations.
    for (const Expr *E : ContractAttr->PreExprs) {
      if (!fillPointersFromExpr(E, ContractAttr->PrePSets))
        continue; // TODO: warn
    }
    for (const Expr *E : ContractAttr->PostExprs) {
      if (!fillPointersFromExpr(E, ContractAttr->PostPSets))
        continue; // TODO: warn
    }
  }

private:
  bool canAssign(QualType From, QualType To) {
    QualType FromPointee = getPointeeType(From);
    if (FromPointee.isNull())
      return false;

    QualType ToPointee = getPointeeType(To);
    if (ToPointee.isNull())
      return false;

    return isConvertible(ASTCtxt.getPointerType(FromPointee),
                         ASTCtxt.getPointerType(ToPointee));
  }

  QualType getLocationType(AttrPSetKey K) {
    QualType Result = FD->getParamDecl(K.first)->getType();
    for (int I = 0; I < K.second; ++I)
      Result = getPointeeType(Result);
    return Result;
  }

  AttrPointsToLoc locFromKey(AttrPSetKey K) {
    AttrPointsToLoc Loc{K.first, {}};
    for (int I = 0; I < K.second; ++I)
      Loc.FDs.push_back(nullptr);
    return Loc;
  }

  struct ParamDerivedLocations {
    std::vector<AttrPSetKey> Input_weak;
    std::vector<AttrPSetKey> Input;
    std::vector<AttrPSetKey> Output;
  };

  const FunctionDecl *FD;
  const ASTContext &ASTCtxt;
  IsConvertibleTy isConvertible;
};

} // anonymous namespace

void getLifetimeContracts(PSetsMap &PMap, const FunctionDecl *FD,
                          const ASTContext &ASTCtxt,
                          IsConvertibleTy isConvertible) {
  auto *ContractAttr = FD->getCanonicalDecl()->getAttr<LifetimeContractAttr>();

  if (ContractAttr->PrePSets.empty()) {
    PSetCollector Collector(FD, ASTCtxt, isConvertible);
    Collector.fillPSetsForDecl(ContractAttr);
  }

  for (const auto &Pair : ContractAttr->PrePSets) {
    Variable V(FD->getParamDecl(Pair.first.first));
    V.deref(Pair.first.second);
    PSet PS(Pair.second, FD->parameters());
    if (const auto *PVD = dyn_cast_or_null<ParmVarDecl>(V.asVarDecl())) {
      if (!V.isField() && !V.isDeref() && PS.containsNull())
        PS.addNullReason(NullReason::parameterNull(PVD->getSourceRange()));
      if (PS.containsInvalid())
        PS = PSet::invalid(
            InvalidationReason::NotInitialized(PVD->getSourceRange()));
    }
    PMap.emplace(V, PS);
  }
  for (const auto &Pair : ContractAttr->PostPSets) {
    Variable V(FD->getParamDecl(Pair.first.first));
    V.deref(Pair.first.second);
    PMap.emplace(V, PSet(Pair.second, FD->parameters()));
  }
  PMap.emplace(Variable::thisPointer(),
               PSet::singleton(Variable::thisPointer()));
}

} // namespace lifetime
} // namespace clang