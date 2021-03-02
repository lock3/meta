//===- TemplateBase.cpp - Common template AST class implementation --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements common classes used throughout C++ template
// representations.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/TemplateBase.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclBase.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/DependenceFlags.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/PackSplice.h"
#include "clang/AST/PrettyPrinter.h"
#include "clang/AST/TemplateName.h"
#include "clang/AST/Type.h"
#include "clang/AST/TypeLoc.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/LLVM.h"
#include "clang/Basic/LangOptions.h"
#include "clang/Basic/SourceLocation.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/ADT/FoldingSet.h"
#include "llvm/ADT/None.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

using namespace clang;

/// Print a template integral argument value.
///
/// \param TemplArg the TemplateArgument instance to print.
///
/// \param Out the raw_ostream instance to use for printing.
///
/// \param Policy the printing policy for EnumConstantDecl printing.
static void printIntegral(const TemplateArgument &TemplArg,
                          raw_ostream &Out, const PrintingPolicy& Policy) {
  const Type *T = TemplArg.getIntegralType().getTypePtr();
  const llvm::APSInt &Val = TemplArg.getAsIntegral();

  if (const EnumType *ET = T->getAs<EnumType>()) {
    for (const EnumConstantDecl* ECD : ET->getDecl()->enumerators()) {
      // In Sema::CheckTemplateArugment, enum template arguments value are
      // extended to the size of the integer underlying the enum type.  This
      // may create a size difference between the enum value and template
      // argument value, requiring isSameValue here instead of operator==.
      if (llvm::APSInt::isSameValue(ECD->getInitVal(), Val)) {
        ECD->printQualifiedName(Out, Policy);
        return;
      }
    }
  }

  if (T->isBooleanType() && !Policy.MSVCFormatting) {
    Out << (Val.getBoolValue() ? "true" : "false");
  } else if (T->isCharType()) {
    const char Ch = Val.getZExtValue();
    Out << ((Ch == '\'') ? "'\\" : "'");
    Out.write_escaped(StringRef(&Ch, 1), /*UseHexEscapes=*/ true);
    Out << "'";
  } else {
    Out << Val;
  }
}

//===----------------------------------------------------------------------===//
// TemplateArgument Implementation
//===----------------------------------------------------------------------===//

TemplateArgument::TemplateArgument(ASTContext &Ctx, const llvm::APSInt &Value,
                                   QualType Type) {
  Integer.Kind = Integral;
  // Copy the APSInt value into our decomposed form.
  Integer.BitWidth = Value.getBitWidth();
  Integer.IsUnsigned = Value.isUnsigned();
  // If the value is large, we have to get additional memory from the ASTContext
  unsigned NumWords = Value.getNumWords();
  if (NumWords > 1) {
    void *Mem = Ctx.Allocate(NumWords * sizeof(uint64_t));
    std::memcpy(Mem, Value.getRawData(), NumWords * sizeof(uint64_t));
    Integer.pVal = static_cast<uint64_t *>(Mem);
  } else {
    Integer.VAL = Value.getZExtValue();
  }

  Integer.Type = Type.getAsOpaquePtr();
}

static PackSplice *CreatePackSplice(ASTContext &Ctx, Expr *Operand,
                                    Optional<unsigned> NumExpansions,
                                    Expr *const *Expansions) {
  if (NumExpansions) {
    return PackSplice::Create(Ctx, Operand,
                              llvm::makeArrayRef(Expansions, *NumExpansions));
  } else {
    return PackSplice::Create(Ctx, Operand);
  }
}

TemplateArgument::TemplateArgument(ASTContext &Ctx, Expr *Operand,
                                   Optional<unsigned> NumExpansions,
                                   Expr *const *Expansions)
   : TemplateArgument(CreatePackSplice(Ctx, Operand, NumExpansions, Expansions))
{ }

TemplateArgument
TemplateArgument::CreatePackCopy(ASTContext &Context,
                                 ArrayRef<TemplateArgument> Args) {
  if (Args.empty())
    return getEmptyPack();

  return TemplateArgument(Args.copy(Context));
}

TemplateArgumentDependence TemplateArgument::getDependence() const {
  auto Deps = TemplateArgumentDependence::None;
  switch (getKind()) {
  case Null:
    return TemplateArgumentDependence::Error;

  case Type:
    Deps = toTemplateArgumentDependence(getAsType()->getDependence());
    if (isa<PackExpansionType>(getAsType()))
      Deps |= TemplateArgumentDependence::Dependent;
    return Deps;

  case Template:
    return toTemplateArgumentDependence(getAsTemplate().getDependence());

  case TemplateExpansion:
    return TemplateArgumentDependence::Dependent |
           TemplateArgumentDependence::Instantiation;

  case Declaration: {
    auto *DC = dyn_cast<DeclContext>(getAsDecl());
    if (!DC)
      DC = getAsDecl()->getDeclContext();
    if (DC->isDependentContext())
      Deps = TemplateArgumentDependence::Dependent |
             TemplateArgumentDependence::Instantiation;
    return Deps;
  }

  case NullPtr:
  case Integral:
    return TemplateArgumentDependence::None;

  case Expression:
    Deps = toTemplateArgumentDependence(getAsExpr()->getDependence());
    return Deps;

  case Pack:
    for (const auto &P : pack_elements())
      Deps |= P.getDependence();
    return Deps;

  case PackSplice: {
    const class PackSplice *PS = getPackSplice();
    if (PS->isExpanded()) {
      for (Expr *E : PS->getExpansions())
        Deps |= toTemplateArgumentDependence(E->getDependence());
      return Deps;
    } else {
      Expr *Operand = PS->getOperand();
      Deps = toTemplateArgumentDependence(Operand->getDependence());
      return Deps;
    }
  }

  }
  llvm_unreachable("unhandled ArgKind");
}

bool TemplateArgument::isDependent() const {
  return getDependence() & TemplateArgumentDependence::Dependent;
}

bool TemplateArgument::isInstantiationDependent() const {
  return getDependence() & TemplateArgumentDependence::Instantiation;
}

bool TemplateArgument::isPackExpansion() const {
  switch (getKind()) {
  case Null:
  case Declaration:
  case Integral:
  case Pack:
  case Template:
  case NullPtr:
    return false;

  case TemplateExpansion:
    return true;

  case Type:
    return isa<PackExpansionType>(getAsType());

  case Expression:
    return isa<PackExpansionExpr>(getAsExpr());

  case PackSplice:
    return !PackSpliceValue.IsPattern;
  }

  llvm_unreachable("Invalid TemplateArgument Kind!");
}

bool TemplateArgument::isConstexprPromoting() const {
  if (getKind() != Type)
    return false;

  return getAsType()->isMetaType();
}

bool TemplateArgument::containsUnexpandedParameterPack() const {
  return getDependence() & TemplateArgumentDependence::UnexpandedPack;
}

Optional<unsigned> TemplateArgument::getNumTemplateExpansions() const {
  assert(getKind() == TemplateExpansion);
  if (TemplateArg.NumExpansions)
    return TemplateArg.NumExpansions - 1;

  return None;
}

QualType TemplateArgument::getNonTypeTemplateArgumentType() const {
  switch (getKind()) {
  case TemplateArgument::Null:
  case TemplateArgument::Type:
  case TemplateArgument::Template:
  case TemplateArgument::TemplateExpansion:
  case TemplateArgument::Pack:
  case TemplateArgument::PackSplice:
    return QualType();

  case TemplateArgument::Integral:
    return getIntegralType();

  case TemplateArgument::Expression:
    return getAsExpr()->getType();

  case TemplateArgument::Declaration:
    return getParamTypeForDecl();

  case TemplateArgument::NullPtr:
    return getNullPtrType();
  }

  llvm_unreachable("Invalid TemplateArgument Kind!");
}

void TemplateArgument::Profile(llvm::FoldingSetNodeID &ID,
                               const ASTContext &Context) const {
  ID.AddInteger(getKind());
  switch (getKind()) {
  case Null:
    break;

  case Type:
    getAsType().Profile(ID);
    break;

  case NullPtr:
    getNullPtrType().Profile(ID);
    break;

  case Declaration:
    getParamTypeForDecl().Profile(ID);
    ID.AddPointer(getAsDecl()? getAsDecl()->getCanonicalDecl() : nullptr);
    break;

  case Template:
  case TemplateExpansion: {
    TemplateName Template = getAsTemplateOrTemplatePattern();
    if (TemplateTemplateParmDecl *TTP
          = dyn_cast_or_null<TemplateTemplateParmDecl>(
                                                Template.getAsTemplateDecl())) {
      ID.AddBoolean(true);
      ID.AddInteger(TTP->getDepth());
      ID.AddInteger(TTP->getPosition());
      ID.AddBoolean(TTP->isParameterPack());
    } else {
      ID.AddBoolean(false);
      ID.AddPointer(Context.getCanonicalTemplateName(Template)
                                                          .getAsVoidPointer());
    }
    break;
  }

  case Integral:
    getAsIntegral().Profile(ID);
    getIntegralType().Profile(ID);
    break;

  case Expression:
    getAsExpr()->Profile(ID, Context, true);
    break;

  case Pack:
    ID.AddInteger(Args.NumArgs);
    for (unsigned I = 0; I != Args.NumArgs; ++I)
      Args.Args[I].Profile(ID, Context);
    break;

  case PackSplice:
    getPackSplice()->Profile(ID, Context);
    break;
  }
}

bool TemplateArgument::structurallyEquals(const TemplateArgument &Other) const {
  if (getKind() != Other.getKind()) return false;

  switch (getKind()) {
  case Null:
  case Type:
  case Expression:
  case NullPtr:
    return TypeOrValue.V == Other.TypeOrValue.V;

  case Template:
  case TemplateExpansion:
    return TemplateArg.Name == Other.TemplateArg.Name &&
           TemplateArg.NumExpansions == Other.TemplateArg.NumExpansions;

  case Declaration:
    return getAsDecl() == Other.getAsDecl();

  case Integral:
    return getIntegralType() == Other.getIntegralType() &&
           getAsIntegral() == Other.getAsIntegral();

  case Pack:
    if (Args.NumArgs != Other.Args.NumArgs) return false;
    for (unsigned I = 0, E = Args.NumArgs; I != E; ++I)
      if (!Args.Args[I].structurallyEquals(Other.Args.Args[I]))
        return false;
    return true;

  case PackSplice: {
    const class PackSplice *PS1 = getPackSplice();
    const class PackSplice *PS2 = Other.getPackSplice();

    if (PS1->isExpanded() != PS2->isExpanded())
      return false;

    if (PS1->isExpanded()) {
      if (PS1->getOperand() != PS2->getOperand())
        return false;
    } else {
      if (PS1->getExpansions() != PS2->getExpansions())
        return false;

      for (unsigned I = 0; I < PS1->getNumExpansions(); ++I)
        if (PS1->getExpansion(I) != PS2->getExpansion(I))
          return false;
    }

    return true;
  }

  }

  llvm_unreachable("Invalid TemplateArgument Kind!");
}

TemplateArgument TemplateArgument::getPackExpansionPattern() const {
  assert(isPackExpansion());

  auto TemplateArgKind = getKind();
  switch (TemplateArgKind) {
  case Type:
    return getAsType()->castAs<PackExpansionType>()->getPattern();

  case Expression:
    return TemplateArgument(cast<PackExpansionExpr>(getAsExpr())->getPattern());

  case TemplateExpansion:
    return TemplateArgument(getAsTemplateOrTemplatePattern());

  case PackSplice:
    return TemplateArgument(getPackSplice(), /*IsPattern=*/true);

  case Declaration:
  case Integral:
  case Pack:
  case Null:
  case Template:
  case NullPtr:
    return TemplateArgument();
  }

  llvm_unreachable("Invalid TemplateArgument Kind!");
}

void TemplateArgument::print(const PrintingPolicy &Policy,
                             raw_ostream &Out) const {
  switch (getKind()) {
  case Null:
    Out << "(no value)";
    break;

  case Type: {
    PrintingPolicy SubPolicy(Policy);
    SubPolicy.SuppressStrongLifetime = true;
    getAsType().print(Out, SubPolicy);
    break;
  }

  case Declaration: {
    NamedDecl *ND = getAsDecl();
    if (getParamTypeForDecl()->isRecordType()) {
      if (auto *TPO = dyn_cast<TemplateParamObjectDecl>(ND)) {
        // FIXME: Include the type if it's not obvious from the context.
        TPO->printAsInit(Out);
        break;
      }
    }
    if (!getParamTypeForDecl()->isReferenceType())
      Out << '&';
    ND->printQualifiedName(Out);
    break;
  }

  case NullPtr:
    Out << "nullptr";
    break;

  case Template:
    getAsTemplate().print(Out, Policy);
    break;

  case TemplateExpansion:
    getAsTemplateOrTemplatePattern().print(Out, Policy);
    Out << "...";
    break;

  case Integral:
    printIntegral(*this, Out, Policy);
    break;

  case Expression:
    getAsExpr()->printPretty(Out, nullptr, Policy);
    break;

  case Pack: {
    Out << "<";
    bool First = true;
    for (const auto &P : pack_elements()) {
      if (First)
        First = false;
      else
        Out << ", ";

      P.print(Policy, Out);
    }
    Out << ">";
    break;
  }

  case PackSplice:
    Out << "...[< ";
    getPackSplice()->getOperand()->printPretty(Out, nullptr, Policy);
    Out << ">]...";
    break;
  }
}

void TemplateArgument::dump(raw_ostream &Out) const {
  LangOptions LO; // FIXME! see also TemplateName::dump().
  LO.CPlusPlus = true;
  LO.Bool = true;
  print(PrintingPolicy(LO), Out);
}

LLVM_DUMP_METHOD void TemplateArgument::dump() const { dump(llvm::errs()); }

//===----------------------------------------------------------------------===//
// TemplateArgumentLoc Implementation
//===----------------------------------------------------------------------===//

SourceRange TemplateArgumentLoc::getSourceRange() const {
  switch (Argument.getKind()) {
  case TemplateArgument::Expression:
    return getSourceExpression()->getSourceRange();

  case TemplateArgument::Declaration:
    return getSourceDeclExpression()->getSourceRange();

  case TemplateArgument::NullPtr:
    return getSourceNullPtrExpression()->getSourceRange();

  case TemplateArgument::Type:
    if (TypeSourceInfo *TSI = getTypeSourceInfo())
      return TSI->getTypeLoc().getSourceRange();
    else
      return SourceRange();

  case TemplateArgument::Template:
    if (getTemplateQualifierLoc())
      return SourceRange(getTemplateQualifierLoc().getBeginLoc(),
                         getTemplateNameLoc());
    return SourceRange(getTemplateNameLoc());

  case TemplateArgument::TemplateExpansion:
    if (getTemplateQualifierLoc())
      return SourceRange(getTemplateQualifierLoc().getBeginLoc(),
                         getTemplateEllipsisLoc());
    return SourceRange(getTemplateNameLoc(), getTemplateEllipsisLoc());

  case TemplateArgument::Integral:
    return getSourceIntegralExpression()->getSourceRange();

  case TemplateArgument::Pack:
  case TemplateArgument::Null:
    return SourceRange();

  case TemplateArgument::PackSplice:
    // FIXME: Improve this
    return SourceRange();

  }

  llvm_unreachable("Invalid TemplateArgument Kind!");
}

SourceLocation TemplateArgumentLoc::getExpansionEllipsisLoc() const {
  if (!Argument.isPackExpansion())
    return SourceLocation();

  switch (Argument.getKind()) {
  case TemplateArgument::Null:
  case TemplateArgument::Declaration:
  case TemplateArgument::Integral:
  case TemplateArgument::Pack:
  case TemplateArgument::Template:
  case TemplateArgument::NullPtr:
    return SourceLocation();

  case TemplateArgument::TemplateExpansion:
    return getTemplateEllipsisLoc();

  case TemplateArgument::PackSplice:
    return LocInfo.getPackSpliceExpansionEllipsisLoc();

  case TemplateArgument::Type: {
    TypeLoc TL = LocInfo.getAsTypeSourceInfo()->getTypeLoc();
    if (auto PETL = TL.getAs<PackExpansionTypeLoc>())
      return PETL.getEllipsisLoc();
    return SourceLocation();
  }

  case TemplateArgument::Expression:
    if (auto *PE = dyn_cast<PackExpansionExpr>(Argument.getAsExpr()))
      return PE->getEllipsisLoc();
    return SourceLocation();
  }

  llvm_unreachable("Invalid TemplateArgument Kind!");
}

template <typename T>
static const T &DiagTemplateArg(const T &DB, const TemplateArgument &Arg) {
  switch (Arg.getKind()) {
  case TemplateArgument::Null:
    // This is bad, but not as bad as crashing because of argument
    // count mismatches.
    return DB << "(null template argument)";

  case TemplateArgument::Type:
    return DB << Arg.getAsType();

  case TemplateArgument::Declaration:
    return DB << Arg.getAsDecl();

  case TemplateArgument::NullPtr:
    return DB << "nullptr";

  case TemplateArgument::Integral:
    return DB << Arg.getAsIntegral().toString(10);

  case TemplateArgument::Template:
    return DB << Arg.getAsTemplate();

  case TemplateArgument::TemplateExpansion:
    return DB << Arg.getAsTemplateOrTemplatePattern() << "...";

  case TemplateArgument::Expression: {
    return DB << Arg.getAsExpr();
  }

  case TemplateArgument::Pack:
  case TemplateArgument::PackSplice: {
    // FIXME: We're guessing at LangOptions!
    SmallString<32> Str;
    llvm::raw_svector_ostream OS(Str);
    LangOptions LangOpts;
    LangOpts.CPlusPlus = true;
    PrintingPolicy Policy(LangOpts);
    Arg.print(Policy, OS);
    return DB << OS.str();
  }

  }

  llvm_unreachable("Invalid TemplateArgument Kind!");
}

const StreamingDiagnostic &clang::operator<<(const StreamingDiagnostic &DB,
                                             const TemplateArgument &Arg) {
  return DiagTemplateArg(DB, Arg);
}

clang::TemplateArgumentLocInfo::TemplateArgumentLocInfo(
    ASTContext &Ctx, NestedNameSpecifierLoc QualifierLoc,
    SourceLocation TemplateNameLoc, SourceLocation EllipsisLoc) {
  TemplateTemplateArgLocInfo *Template = new (Ctx) TemplateTemplateArgLocInfo;
  Template->Qualifier = QualifierLoc.getNestedNameSpecifier();
  Template->QualifierLocData = QualifierLoc.getOpaqueData();
  Template->TemplateNameLoc = TemplateNameLoc;
  Template->EllipsisLoc = EllipsisLoc;
  Pointer = Template;
}

clang::TemplateArgumentLocInfo::TemplateArgumentLocInfo(
    ASTContext &Ctx, SourceLocation IntroEllipsisLoc,
    SourceLocation SBELoc, SourceLocation SEELoc,
    SourceLocation ExpansionEllipsisLoc) {
  auto *PackSplice = new (Ctx) PackSpliceArgLocInfo;
  PackSplice->IntroEllipsisLoc = IntroEllipsisLoc.getRawEncoding();
  PackSplice->SBELoc = SBELoc.getRawEncoding();
  PackSplice->SEELoc = SEELoc.getRawEncoding();
  PackSplice->ExpansionEllipsisLoc = ExpansionEllipsisLoc.getRawEncoding();
  Pointer = PackSplice;
}

const ASTTemplateArgumentListInfo *
ASTTemplateArgumentListInfo::Create(const ASTContext &C,
                                    const TemplateArgumentListInfo &List) {
  std::size_t size = totalSizeToAlloc<TemplateArgumentLoc>(List.size());
  void *Mem = C.Allocate(size, alignof(ASTTemplateArgumentListInfo));
  return new (Mem) ASTTemplateArgumentListInfo(List);
}

ASTTemplateArgumentListInfo::ASTTemplateArgumentListInfo(
    const TemplateArgumentListInfo &Info) {
  LAngleLoc = Info.getLAngleLoc();
  RAngleLoc = Info.getRAngleLoc();
  NumTemplateArgs = Info.size();

  TemplateArgumentLoc *ArgBuffer = getTrailingObjects<TemplateArgumentLoc>();
  for (unsigned i = 0; i != NumTemplateArgs; ++i)
    new (&ArgBuffer[i]) TemplateArgumentLoc(Info[i]);
}

void ASTTemplateKWAndArgsInfo::initializeFrom(
    SourceLocation TemplateKWLoc, const TemplateArgumentListInfo &Info,
    TemplateArgumentLoc *OutArgArray) {
  this->TemplateKWLoc = TemplateKWLoc;
  LAngleLoc = Info.getLAngleLoc();
  RAngleLoc = Info.getRAngleLoc();
  NumTemplateArgs = Info.size();

  for (unsigned i = 0; i != NumTemplateArgs; ++i)
    new (&OutArgArray[i]) TemplateArgumentLoc(Info[i]);
}

void ASTTemplateKWAndArgsInfo::initializeFrom(SourceLocation TemplateKWLoc) {
  assert(TemplateKWLoc.isValid());
  LAngleLoc = SourceLocation();
  RAngleLoc = SourceLocation();
  this->TemplateKWLoc = TemplateKWLoc;
  NumTemplateArgs = 0;
}

void ASTTemplateKWAndArgsInfo::initializeFrom(
    SourceLocation TemplateKWLoc, const TemplateArgumentListInfo &Info,
    TemplateArgumentLoc *OutArgArray, TemplateArgumentDependence &Deps) {
  this->TemplateKWLoc = TemplateKWLoc;
  LAngleLoc = Info.getLAngleLoc();
  RAngleLoc = Info.getRAngleLoc();
  NumTemplateArgs = Info.size();

  for (unsigned i = 0; i != NumTemplateArgs; ++i) {
    Deps |= Info[i].getArgument().getDependence();

    new (&OutArgArray[i]) TemplateArgumentLoc(Info[i]);
  }
}

void ASTTemplateKWAndArgsInfo::copyInto(const TemplateArgumentLoc *ArgArray,
                                        TemplateArgumentListInfo &Info) const {
  Info.setLAngleLoc(LAngleLoc);
  Info.setRAngleLoc(RAngleLoc);
  for (unsigned I = 0; I != NumTemplateArgs; ++I)
    Info.addArgument(ArgArray[I]);
}
