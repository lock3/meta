
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

static AttrPointsToSet singleton(AttrPointsToLoc Loc) {
  AttrPointsToSet Ret;
  Ret.Pointees.push_back(Loc);
  return Ret;
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
         !Set.HasStatic;
}


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

static AttrPointsToSet collectPSet(const Expr *E,
                                   const AttrPointsToMap &Lookup) {
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
      auto It = Lookup.find(Key);
      assert(It != Lookup.end());
      return It->second;
    }
    return Result;
  } else if (const auto *StdInit = dyn_cast<CXXStdInitializerListExpr>(E)) {
    E = StdInit->getSubExpr()->IgnoreImplicit();
    if (const auto *InitList = dyn_cast<InitListExpr>(E)) {
      for (const auto *Init : InitList->inits()) {
        AttrPointsToSet Elem = collectPSet(ignoreReturnValues(Init), Lookup);
        if (isEmpty(Elem))
          return Elem;
        Result = merge(Result, Elem);
      }
    }
  }
  return Result;
}

static unsigned getDeclIndex(const ValueDecl *D) {
  if (const auto *PVD = dyn_cast<ParmVarDecl>(D))
    return PVD->getFunctionScopeIndex();
  else if (const auto *VD = dyn_cast<VarDecl>(D)) {
    StringRef Name = VD->getName();
    if (Name == "Return")
      return AttrPointsToLoc::ReturnVal;
    else if (Name == "This")
      return AttrPointsToLoc::ThisVal;
  }
  llvm_unreachable("Unexpected declaration.");
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
static bool fillPointersFromExpr(const Expr *E, AttrPointsToMap &Fill,
                                 const AttrPointsToMap &Lookup) {
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

  AttrPSetKey VD(getDeclIndex(cast<DeclRefExpr>(LHS)->getDecl()), 0);
  AttrPointsToSet PSet = collectPSet(RHS, Lookup);
  if (isEmpty(PSet))
    return false;
  Fill[VD] = PSet;
  return true;
}

namespace {
static Variable varFromPSetKey(AttrPSetKey K, const FunctionDecl *FD) {
  // Temporaries cannot occur in contracts, so we use a temporary to represen
  // contract of return values. A bit cleaner solution in the future might be
  // extending the PointerUnion inside Var or having a representation of the
  // contracts that is completely independent of the analysis.
  if (K.first == AttrPointsToLoc::ReturnVal)
    return Variable::temporary().deref(K.second);
  else if (K.first == AttrPointsToLoc::ThisVal)
    return Variable::thisPointer().deref(K.second);
  return Variable(FD->getParamDecl(K.first)).deref(K.second);
}

class PSetCollector {
public:
  PSetCollector(const FunctionDecl *FD, const ASTContext &ASTCtxt,
                IsConvertibleTy isConvertible, LifetimeReporterBase &Reporter)
      : FD(FD), ASTCtxt(ASTCtxt), isConvertible(isConvertible),
        Reporter(Reporter) {}

  void fillPSetsForDecl(LifetimeContractAttr *ContractAttr) {
    // Fill default preconditions and collect data for
    // computing default postconditions.
    ParamDerivedLocations Locations;
    for (const ParmVarDecl *PVD : FD->parameters()) {
      QualType ParamTy = PVD->getType();
      TypeCategory TC = classifyTypeCategory(ParamTy);
      if (TC != TypeCategory::Pointer && TC != TypeCategory::Owner)
        continue;

      AttrPSetKey ParamLoc(PVD->getFunctionScopeIndex(), 0);
      AttrPointsToSet ParamPSet = singleton({ParamLoc.first, {nullptr}});
      // Nullable owners are a future note in the paper.
      ParamPSet.HasNull = isNullableType(ParamTy);
      ContractAttr->PrePSets.try_emplace(ParamLoc, ParamPSet);
      if (TC != TypeCategory::Pointer)
        continue;

      AttrPSetKey ParamDerefLoc(ParamLoc.first, 1);
      QualType PointeeType = getPointeeType(ParamTy);
      AttrPointsToSet DerefPS = singleton({ParamLoc.first, {nullptr, nullptr}});
      DerefPS.HasNull = isNullableType(PointeeType);
      switch (classifyTypeCategory(PointeeType)) {
      case TypeCategory::Owner: {
        ContractAttr->PrePSets.try_emplace(ParamDerefLoc, DerefPS);
        if (ParamTy->isLValueReferenceType()) {
          if (PointeeType.isConstQualified()) {
            Locations.Input_weak.push_back(ParamLoc);
            Locations.Input_weak.push_back(ParamDerefLoc);
          } else {
            Locations.Input.push_back(ParamLoc);
            Locations.Input.push_back(ParamDerefLoc);
          }
        }
        break;
      }
      case TypeCategory::Pointer:
        if (!PointeeType.isConstQualified()) {
          // Output params are initially invalid.
          AttrPointsToSet InvalidPS;
          InvalidPS.HasInvalid = true;
          ContractAttr->PrePSets.try_emplace(ParamDerefLoc, InvalidPS);
          Locations.Output.push_back(ParamDerefLoc);
        } else {
          ContractAttr->PrePSets.try_emplace(ParamDerefLoc, DerefPS);
          // TODO: In the paper we only add derefs for references and not for
          // other pointers. Is this intentional?
          if (ParamTy->isLValueReferenceType())
            Locations.Input.push_back(ParamDerefLoc);
        }
        LLVM_FALLTHROUGH;
      default:
        if (!ParamTy->isRValueReferenceType())
          Locations.Input.push_back(ParamLoc);
        break;
      }
    }
    // This points to deref this and this considered as input.
    if (const auto *MD = dyn_cast<CXXMethodDecl>(FD)) {
      AttrPointsToSet ThisPS = singleton({AttrPointsToLoc::ThisVal, {nullptr}});
      ContractAttr->PrePSets.try_emplace({AttrPointsToLoc::ThisVal, 0}, ThisPS);
      Locations.Input.push_back({AttrPointsToLoc::ThisVal, 0});
      QualType ClassTy = MD->getThisType()->getPointeeType();
      TypeCategory TC = classifyTypeCategory(ClassTy);
      if (TC == TypeCategory::Pointer || TC == TypeCategory::Owner) {
        AttrPointsToSet DerefThisPS =
            singleton({AttrPointsToLoc::ThisVal, {nullptr, nullptr}});
        auto OO = MD->getOverloadedOperator();
        if (OO != OverloadedOperatorKind::OO_Star &&
            OO != OverloadedOperatorKind::OO_Arrow &&
            OO != OverloadedOperatorKind::OO_ArrowStar &&
            OO != OverloadedOperatorKind::OO_Subscript)
          DerefThisPS.HasNull = isNullableType(ClassTy);
        if (const auto *Conv = dyn_cast<CXXConversionDecl>(MD))
          DerefThisPS.HasNull |= Conv->getConversionType()->isBooleanType();
        ContractAttr->PrePSets.try_emplace({AttrPointsToLoc::ThisVal, 1},
                                           DerefThisPS);
        Locations.Input.push_back({AttrPointsToLoc::ThisVal, 1});
      }
    }

    // Adust preconditions based on annotations.
    for (const Expr *E : ContractAttr->PreExprs) {
      if (!fillPointersFromExpr(E, ContractAttr->PrePSets,
                                ContractAttr->PrePSets))
        Reporter.warnUnsupportedExpr(E->getSourceRange());
    }

    // Compute default postconditions.
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

    if (classifyTypeCategory(FD->getReturnType()) == TypeCategory::Pointer)
      ContractAttr->PostPSets[{AttrPointsToLoc::ReturnVal, 0}] =
          computeOutput(FD->getReturnType());

    // Process user defined postconditions.
    for (const Expr *E : ContractAttr->PostExprs) {
      if (!fillPointersFromExpr(E, ContractAttr->PostPSets,
                                ContractAttr->PrePSets))
        Reporter.warnUnsupportedExpr(E->getSourceRange());
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
    QualType Result;
    if (K.first == AttrPointsToLoc::ThisVal)
      Result = cast<CXXMethodDecl>(FD)->getThisType();
    else
      Result = FD->getParamDecl(K.first)->getType();
    for (int I = 0; I < K.second; ++I)
      Result = getPointeeType(Result);
    return Result;
  }

  struct ParamDerivedLocations {
    std::vector<AttrPSetKey> Input_weak;
    std::vector<AttrPSetKey> Input;
    std::vector<AttrPSetKey> Output;
  };

  const FunctionDecl *FD;
  const ASTContext &ASTCtxt;
  IsConvertibleTy isConvertible;
  LifetimeReporterBase &Reporter;
};

} // anonymous namespace

void getLifetimeContracts(PSetsMap &PMap, const FunctionDecl *FD,
                          const ASTContext &ASTCtxt,
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
      Variable V(varFromPSetKey(Pair.first, FD));
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
  } else {
    for (const auto &Pair : ContractAttr->PostPSets)
      PMap.emplace(varFromPSetKey(Pair.first, FD),
                   PSet(Pair.second, FD->parameters()));
  }
}

} // namespace lifetime
} // namespace clang