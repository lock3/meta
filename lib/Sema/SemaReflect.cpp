//===--- SemaReflect.cpp - Semantic Analysis for Reflection ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements semantic analysis for reflection.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTContext.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/Decl.h"
#include "clang/Basic/PartialDiagnostic.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Sema/Lookup.h"
#include "clang/Sema/ParsedTemplate.h"
#include "clang/Sema/ParsedReflection.h"
#include "clang/Sema/Scope.h"
#include "clang/Sema/SemaInternal.h"
#include "TypeLocBuilder.h"
using namespace clang;
using namespace sema;

ParsedReflectionOperand Sema::ActOnReflectedType(TypeResult T) {
  // Cheat by funneling this back through template argument processing
  // because it's possible to end up with deduction guides.
  ParsedTemplateArgument Arg = ActOnTemplateTypeArgument(T);
  if (Arg.getKind() == ParsedTemplateArgument::Template)
    return ActOnReflectedTemplate(Arg);
  assert(Arg.getKind() == ParsedTemplateArgument::Type);

  // llvm::outs() << "GOT TYPE\n";
  // T.get().get()->dump();
  return ParsedReflectionOperand(T.get(), Arg.getLocation());
}

ParsedReflectionOperand Sema::ActOnReflectedTemplate(ParsedTemplateArgument T) {
  assert(T.getKind() == ParsedTemplateArgument::Template);
  const CXXScopeSpec& SS = T.getScopeSpec();
  ParsedTemplateTy Temp = T.getAsTemplate();
  
  // llvm::outs() << "GOT TEMPLATE\n";
  // Temp.get().getAsTemplateDecl()->dump();
  return ParsedReflectionOperand(SS, Temp, T.getLocation());
}

ParsedReflectionOperand Sema::ActOnReflectedNamespace(CXXScopeSpec &SS,
                                                      SourceLocation &Loc,
                                                      Decl *D) {
  // llvm::outs() << "GOT NAMESPACE\n";
  // D->dump();
  return ParsedReflectionOperand(SS, D, Loc);
}

ParsedReflectionOperand Sema::ActOnReflectedExpression(Expr *E) {
  
  // llvm::outs() << "GOT EXPRESSION\n";
  // E->dump();
  return ParsedReflectionOperand(E, E->getBeginLoc());
}

/// Returns a constant expression that encodes the value of the reflection.
/// The type of the reflection is meta::reflection, an enum class.
ExprResult Sema::ActOnCXXReflectExpr(SourceLocation Loc,
                                     ParsedReflectionOperand Ref, 
                                     SourceLocation LP, 
                                     SourceLocation RP) {
  // Translated the parsed reflection operand into an AST operand.
  switch (Ref.getKind()) {
  case ParsedReflectionOperand::Invalid:
    break;
  case ParsedReflectionOperand::Type: {
    QualType Arg = Ref.getAsType().get();
    return BuildCXXReflectExpr(Loc, Arg, LP, RP);
  }
  case ParsedReflectionOperand::Template: {
    TemplateName Arg = Ref.getAsTemplate().get();
    return BuildCXXReflectExpr(Loc, Arg, LP, RP);
  }
  case ParsedReflectionOperand::Namespace: {
    // FIXME: If the ScopeSpec is non-empty, create a qualified namespace-name.
    NamespaceName Arg(cast<NamespaceDecl>(Ref.getAsNamespace()));
    return BuildCXXReflectExpr(Loc, Arg, LP, RP);
  }
  case ParsedReflectionOperand::Expression: {
    Expr *Arg = Ref.getAsExpr();
    return BuildCXXReflectExpr(Loc, Arg, LP, RP);
  }
  }

  return ExprError();
}

ExprResult Sema::BuildCXXReflectExpr(SourceLocation Loc, QualType T,
                                     SourceLocation LP, SourceLocation RP) {
  return CXXReflectExpr::Create(Context, Context.MetaInfoTy, Loc, T, LP, RP);
}

ExprResult Sema::BuildCXXReflectExpr(SourceLocation Loc, TemplateName N,
                                     SourceLocation LP, SourceLocation RP) {
  return CXXReflectExpr::Create(Context, Context.MetaInfoTy, Loc, N, LP, RP);
}

ExprResult Sema::BuildCXXReflectExpr(SourceLocation Loc, NamespaceName N,
                                     SourceLocation LP, SourceLocation RP) {
  return CXXReflectExpr::Create(Context, Context.MetaInfoTy, Loc, N, LP, RP);
}

ExprResult Sema::BuildCXXReflectExpr(SourceLocation Loc, Expr *E,
                                     SourceLocation LP, SourceLocation RP) {
  return CXXReflectExpr::Create(Context, Context.MetaInfoTy, Loc, E, LP, RP);
}

static bool SetType(QualType& Ret, QualType T) {
  Ret = T;
  return true;
}

static bool SetCStrType(QualType& Ret, ASTContext &Ctx) {
  return SetType(Ret, Ctx.getPointerType(Ctx.getConstType(Ctx.CharTy)));
}

// Check that the argument has the right type. Ignore references and
// cv-qualifiers on the expression.
static bool CheckReflectionOperand(Sema &SemaRef, Expr *E) {
  // Get the type of the expression.
  QualType Source = E->getType();
  if (Source->isReferenceType())
    Source = Source->getPointeeType();
  Source = Source.getUnqualifiedType();
  Source = SemaRef.Context.getCanonicalType(Source);

  if (Source != SemaRef.Context.MetaInfoTy) {
    SemaRef.Diag(E->getBeginLoc(), diag::err_expression_not_reflection);
    return false;
  }

  return true;
}

// Gets the type and query from Arg. Returns false on error.
static bool GetTypeAndQuery(Sema &SemaRef, Expr *&Arg, QualType &ResultTy, 
                            ReflectionQuery& Query) {
  // Guess these values initially.
  ResultTy = SemaRef.Context.DependentTy;
  Query = RQ_unknown;

  if (Arg->isTypeDependent() || Arg->isValueDependent())
    return true;

  // Convert to an rvalue.
  ExprResult Conv = SemaRef.DefaultLvalueConversion(Arg);
  if (Conv.isInvalid())
    return false;
  Arg = Conv.get();

  // FIXME: Should what type should this actually be?
  QualType T = Arg->getType();
  if (T->isIntegralType(SemaRef.Context)) {
    SemaRef.Diag(Arg->getExprLoc(), diag::err_reflection_query_wrong_type);
    return false;
  }

  // Evaluate the query operand
  SmallVector<PartialDiagnosticAt, 4> Diags;
  Expr::EvalResult Result;
  Result.Diag = &Diags;
  if (!Arg->EvaluateAsRValue(Result, SemaRef.Context)) {
    // FIXME: This is not the right error.
    SemaRef.Diag(Arg->getExprLoc(), diag::err_reflection_query_not_constant);
    for (PartialDiagnosticAt PD : Diags)
      SemaRef.Diag(PD.first, PD.second);
    return false;
  }

  Query = static_cast<ReflectionQuery>(Result.Val.getInt().getExtValue());

  if (isPredicateQuery(Query)) 
    return SetType(ResultTy, SemaRef.Context.BoolTy);
  if (isTraitQuery(Query))
    return SetType(ResultTy, SemaRef.Context.UnsignedIntTy);
  if (isAssociatedReflectionQuery(Query))
    return SetType(ResultTy, SemaRef.Context.MetaInfoTy);
  if (isNameQuery(Query))
    return SetCStrType(ResultTy, SemaRef.Context);

  SemaRef.Diag(Arg->getExprLoc(), diag::err_reflection_query_invalid);
  return false;
}

ExprResult Sema::ActOnCXXReflectionTrait(SourceLocation KWLoc,
                                         SmallVectorImpl<Expr *> &Args,
                                         SourceLocation LParenLoc,
                                         SourceLocation RParenLoc) {
  // FIXME: There are likely unary and n-ary queries.
  if (Args.size() != 2) {
    Diag(KWLoc, diag::err_reflection_wrong_arity) << (int)Args.size();
    return ExprError();
  }

  // Get the type of the query. Note that this will convert Args[0].
  QualType Ty;
  ReflectionQuery Query;
  if (!GetTypeAndQuery(*this, Args[0], Ty, Query))
    return ExprError();

  // Convert the remaining operands to rvalues.
  for (std::size_t I = 1; I < Args.size(); ++I) {
    ExprResult Arg = DefaultLvalueConversion(Args[I]);
    if (Arg.isInvalid())
      return ExprError();
    Args[I] = Arg.get();
  }

  return new (Context) CXXReflectionTraitExpr(Context, Ty, Query, Args, 
                                              KWLoc, LParenLoc, RParenLoc);
}

static bool AppendStringValue(Sema& S, llvm::raw_ostream& OS,
                              const APValue& Val) {
  // Extracting the string valkue from the LValue.
  //
  // FIXME: We probably want something like EvaluateAsString in the Expr class.
  APValue::LValueBase Base = Val.getLValueBase();
  if (Base.is<const Expr *>()) {
    const Expr *BaseExpr = Base.get<const Expr *>();
    assert(isa<StringLiteral>(BaseExpr) && "Not a string literal");
    const StringLiteral *Str = cast<StringLiteral>(BaseExpr);
    OS << Str->getString();
  } else {
    llvm_unreachable("Use of string variable not implemented");
    // const ValueDecl *D = Base.get<const ValueDecl *>();
    // return Error(E->getMessage());
  }
  return true;
}


static bool AppendCharacterArray(Sema& S, llvm::raw_ostream &OS, Expr *E,
                                 QualType T) {
  assert(T->isArrayType() && "Not an array type");
  const ArrayType *ArrayTy = cast<ArrayType>(T.getTypePtr());

  // Check that the type is 'const char[N]' or 'char[N]'.
  QualType ElemTy = ArrayTy->getElementType();
  if (!ElemTy->isCharType()) {
    S.Diag(E->getBeginLoc(), diag::err_reflected_id_invalid_operand_type) << T;
    return false;
  }

  // Evaluate the expression.
  Expr::EvalResult Result;
  if (!E->EvaluateAsLValue(Result, S.Context)) {
    // FIXME: Include notes in the diagnostics.
    S.Diag(E->getBeginLoc(), diag::err_expr_not_ice) << 1;
    return false;
  }

  return AppendStringValue(S, OS, Result.Val);
}

static bool AppendCharacterPointer(Sema& S, llvm::raw_ostream &OS, Expr *E,
                                   QualType T) {
  assert(T->isPointerType() && "Not a pointer type");
  const PointerType* PtrTy = cast<PointerType>(T.getTypePtr());

  // Check for 'const char*'.
  QualType ElemTy = PtrTy->getPointeeType();
  if (!ElemTy->isCharType() || !ElemTy.isConstQualified()) {
    S.Diag(E->getBeginLoc(), diag::err_reflected_id_invalid_operand_type) << T;
    return false;
  }

  // Try evaluating the expression as an rvalue and then extract the result.
  Expr::EvalResult Result;
  if (!E->EvaluateAsRValue(Result, S.Context)) {
    // FIXME: This is not the right error.
    S.Diag(E->getBeginLoc(), diag::err_expr_not_ice) << 1;
    return false;
  }

  return AppendStringValue(S, OS, Result.Val);
}

static bool AppendInteger(Sema& S, llvm::raw_ostream &OS, Expr *E, QualType T) {
  llvm::APSInt N;
  if (!E->EvaluateAsInt(N, S.Context)) {
    S.Diag(E->getBeginLoc(), diag::err_expr_not_ice) << 1;
    return false;
  }
  OS << N;
  return true;
}

static inline bool
AppendReflectedDecl(Sema &S, llvm::raw_ostream &OS, const Expr *ReflExpr,
                    const Decl *D) {
  // If this is a named declaration, append its identifier.
  if (!isa<NamedDecl>(D)) {
    // FIXME: Improve diagnostics.
    S.Diag(ReflExpr->getBeginLoc(), diag::err_reflection_not_named);
    return false;
  }
  const NamedDecl *ND = cast<NamedDecl>(D);

  // FIXME: What if D has a special name? For example operator==?
  // What would we append in that case?
  DeclarationName Name = ND->getDeclName();
  if (!Name.isIdentifier()) {
    S.Diag(ReflExpr->getBeginLoc(), diag::err_reflected_id_not_an_identifer) << Name;
    return false;
  }

  OS << ND->getName();
  return true;
}

static inline bool
AppendReflectedType(Sema& S, llvm::raw_ostream &OS, const Expr *ReflExpr,
                    const QualType &QT) {
  const Type *T = QT.getTypePtr();

  // If this is a class type, append its identifier.
  if (auto *RC = T->getAsCXXRecordDecl())
    OS << RC->getName();
  else if (const BuiltinType *BT = T->getAs<BuiltinType>())
    OS << BT->getName();
  else {
    S.Diag(ReflExpr->getBeginLoc(), diag::err_reflected_id_not_an_identifer)
      << QualType(T, 0);
    return false;
  }
  return true;
}

static Reflection EvaluateReflection(Sema &S, Expr *E) {
  SmallVector<PartialDiagnosticAt, 4> Diags;
  Expr::EvalResult Result;
  Result.Diag = &Diags;
  if (!E->EvaluateAsRValue(Result, S.Context)) {
    S.Diag(E->getExprLoc(), diag::reflection_not_constant_expression);
    for (PartialDiagnosticAt PD : Diags)
      S.Diag(PD.first, PD.second);
    return Reflection();
  }

  return Reflection(S.Context, Result.Val);
}

static bool
AppendReflection(Sema& S, llvm::raw_ostream &OS, Expr *E) {
  Reflection Refl = EvaluateReflection(S, E);

  switch(Refl.getKind()) {
  case RK_invalid:
    llvm_unreachable("Should already be validated");

  case RK_type: {
    const QualType QT = Refl.getAsType();
    return AppendReflectedType(S, OS, E, QT);
  }

  case RK_declaration: {
    const Decl *D = Refl.getAsDeclaration();
    return AppendReflectedDecl(S, OS, E, D);
  }

  case RK_expression: {
    const Expr *RE = Refl.getAsExpression();
    if (const DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(RE))
      return AppendReflectedDecl(S, OS, E, DRE->getDecl());
  }

  case RK_base_specifier:
    break;
  }

  llvm_unreachable("Unsupported reflection type");
}

static bool HasDependentParts(SmallVectorImpl<Expr *>& Parts) {
 return std::any_of(Parts.begin(), Parts.end(), [](const Expr *E) {
   return E->isTypeDependent() || E->isValueDependent();
 });
}

/// Constructs a new identifier from the expressions in Parts. Returns nullptr
/// on error.
DeclarationNameInfo Sema::BuildReflectedIdName(SourceLocation BeginLoc,
                                               SmallVectorImpl<Expr *> &Parts,
                                               SourceLocation EndLoc) {

  // If any components are dependent, we can't compute the name.
  if (HasDependentParts(Parts)) {
    DeclarationName Name
      = Context.DeclarationNames.getCXXReflectedIdName(Parts.size(), &Parts[0]);
    DeclarationNameInfo NameInfo(Name, BeginLoc);
    NameInfo.setCXXReflectedIdNameRange({BeginLoc, EndLoc});
    return NameInfo;
  }

  SmallString<256> Buf;
  llvm::raw_svector_ostream OS(Buf);
  for (std::size_t I = 0; I < Parts.size(); ++I) {
    Expr *E = Parts[I];

    assert(!E->isTypeDependent() && !E->isValueDependent()
        && "Dependent name component");

    // Get the type of the reflection.
    QualType T = E->getType();
    if (AutoType *D = T->getContainedAutoType()) {
      T = D->getDeducedType();
      if (!T.getTypePtr())
        llvm_unreachable("Undeduced value reflection");
    }
    T = Context.getCanonicalType(T);

    SourceLocation ExprLoc = E->getBeginLoc();

    // Evaluate the sub-expression (depending on type) in order to compute
    // a string part that will constitute a declaration name.
    if (T->isConstantArrayType()) {
      if (!AppendCharacterArray(*this, OS, E, T))
        return DeclarationNameInfo();
    }
    else if (T->isPointerType()) {
      if (!AppendCharacterPointer(*this, OS, E, T))
        return DeclarationNameInfo();
    }
    else if (T->isIntegerType()) {
      if (I == 0) {
        // An identifier cannot start with an integer value.
        Diag(ExprLoc, diag::err_reflected_id_with_integer_prefix);
        return DeclarationNameInfo();
      }
      if (!AppendInteger(*this, OS, E, T))
        return DeclarationNameInfo();
    }
    else if (CheckReflectionOperand(*this, E)) {
      if (!AppendReflection(*this, OS, E))
        return DeclarationNameInfo();
    }
    else {
      Diag(ExprLoc, diag::err_reflected_id_invalid_operand_type) << T;
      return DeclarationNameInfo();
    }
  }

  // FIXME: Should we always return a declaration name?
  IdentifierInfo *Id = &PP.getIdentifierTable().get(Buf);
  DeclarationName Name = Context.DeclarationNames.getIdentifier(Id);
  return DeclarationNameInfo(Name, BeginLoc);
}

/// Constructs a new identifier from the expressions in Parts. Returns false
/// if no errors were encountered.
bool Sema::BuildDeclnameId(SmallVectorImpl<Expr *> &Parts,
                           UnqualifiedId &Result,
                           SourceLocation KWLoc,
                           SourceLocation RParenLoc) {
  DeclarationNameInfo NameInfo = BuildReflectedIdName(KWLoc, Parts, RParenLoc);
  DeclarationName Name = NameInfo.getName();
  if (!Name)
    return true;
  if (Name.getNameKind() == DeclarationName::CXXReflectedIdName)
    Result.setReflectedId(KWLoc, Name.getCXXReflectedIdArguments(),
                          RParenLoc);
  else
    Result.setIdentifier(Name.getAsIdentifierInfo(), KWLoc);
  return false;
}

ExprResult Sema::ActOnCXXUnreflexprExpression(SourceLocation Loc,
                                              Expr *Reflection) {
  return BuildCXXUnreflexprExpression(Loc, Reflection);
}

ExprResult Sema::BuildCXXUnreflexprExpression(SourceLocation Loc,
                                              Expr *E) {
  // TODO: Process the reflection E, UnresolveLookupExpr

  // Don't act on dependent expressions, just preserve them.
  // if (E->isTypeDependent() || E->isValueDependent())
  //   return new (Context) CXXUnreflexprExpr(E, Context.DependentTy,
  //                                          VK_RValue, OK_Ordinary, Loc);

  // The operand must be a reflection.
  // if (!CheckReflectionOperand(*this, E))
  //   return ExprError();

  // TODO: Process the reflection E, into DeclRefExpr

  // CXXUnreflexprExpr *Val =
  //     new (Context) CXXUnreflexprExpr(E, E->getType(), E->getValueKind(),
  //                                     E->getObjectKind(), Loc);

  // return Val;
  return ExprError();
}

/// Evaluates the given expression and yields the computed type.
QualType Sema::BuildReflectedType(SourceLocation TypenameLoc, Expr *E) {
  if (E->isTypeDependent() || E->isValueDependent())
    return Context.getReflectedType(E, Context.DependentTy);

  // The operand must be a reflection.
  if (!CheckReflectionOperand(*this, E))
    return QualType();

  Reflection Refl = EvaluateReflection(*this, E);
  if (Refl.isInvalid())
    return QualType();

  if (!Refl.isType()) {
    Diag(E->getExprLoc(), diag::err_expression_not_type_reflection);
    return QualType();
  }

  // Get the type of the reflected entity.
  QualType Reflected = Refl.getAsType();

  return Context.getReflectedType(E, Reflected);
}

/// Evaluates the given expression and yields the computed type.
TypeResult Sema::ActOnReflectedTypeSpecifier(SourceLocation TypenameLoc,
                                             Expr *E) {
  QualType T = BuildReflectedType(TypenameLoc, E);
  if (T.isNull())
    return TypeResult(true);

  // FIXME: Add parens?
  TypeLocBuilder TLB;
  ReflectedTypeLoc TL = TLB.push<ReflectedTypeLoc>(T);
  TL.setNameLoc(TypenameLoc);
  TypeSourceInfo *TSI = TLB.getTypeSourceInfo(Context, T);
  return CreateParsedType(T, TSI);
}

static ParsedTemplateArgument
BuildDependentTemplarg(SourceLocation KWLoc, Expr *ReflExpr) {
  void *OpaqueReflexpr = reinterpret_cast<void *>(ReflExpr);
  return ParsedTemplateArgument(ParsedTemplateArgument::Dependent,
                                OpaqueReflexpr, KWLoc);
}

static ParsedTemplateArgument
DiagnoseDeclTemplarg(Sema &S, SourceLocation KWLoc,
                     Expr *ReflExpr, const Decl *D) {
  llvm::outs() << "Templarg - Decl\n";
  return ParsedTemplateArgument();
}

static ParsedTemplateArgument
BuildReflectedTypeTemplarg(Sema &S, SourceLocation KWLoc,
                           Expr *ReflExpr, const QualType &QT) {
  llvm::outs() << "Templarg - Type\n";
  const Type *T = QT.getTypePtr();
  void *OpaqueT = reinterpret_cast<void *>(const_cast<Type *>(T));
  return ParsedTemplateArgument(ParsedTemplateArgument::Type, OpaqueT, KWLoc);
}

static ParsedTemplateArgument
BuildReflectedExprTemplarg(Sema &S, SourceLocation KWLoc,
                           Expr *E, const Expr *RE) {
  llvm::outs() << "Templarg - Expr\n";
  void *OpaqueRE = reinterpret_cast<void *>(const_cast<Expr *>(RE));
  return ParsedTemplateArgument(ParsedTemplateArgument::NonType, OpaqueRE,
                                KWLoc);
}

ParsedTemplateArgument
Sema::ActOnReflectedTemplateArgument(SourceLocation KWLoc, Expr *E) {
  if (E->isTypeDependent() || E->isValueDependent())
    return BuildDependentTemplarg(KWLoc, E);

  if (!CheckReflectionOperand(*this, E))
    return ParsedTemplateArgument();

  Reflection Refl = EvaluateReflection(*this, E);

  switch(Refl.getKind()) {
  case RK_invalid:
    return ParsedTemplateArgument();

  case RK_type: {
    const QualType QT = Refl.getAsType();
    return BuildReflectedTypeTemplarg(*this, KWLoc, E, QT);
  }

  case RK_declaration: {
    const Decl *D = Refl.getAsDeclaration();
    return DiagnoseDeclTemplarg(*this, KWLoc, E, D);
  }

  case RK_expression: {
    const Expr *RE = Refl.getAsExpression();
    return BuildReflectedExprTemplarg(*this, KWLoc, E, RE);
  }

  case RK_base_specifier:
    break;
  }

  llvm_unreachable("Unsupported reflection type");
}

