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

static bool HasDependentParts(SmallVectorImpl<Expr *>& Parts) {
 return std::any_of(Parts.begin(), Parts.end(), [](const Expr *E) {
   return E->isTypeDependent() || E->isValueDependent();
 });
}

ExprResult Sema::ActOnCXXReflectPrintLiteral(SourceLocation KWLoc,
                                             SmallVectorImpl<Expr *> &Args,
                                             SourceLocation LParenLoc,
                                             SourceLocation RParenLoc) {
  if (HasDependentParts(Args))
    return new (Context) CXXReflectPrintLiteralExpr(
        Context, Context.DependentTy, Args, KWLoc, LParenLoc, RParenLoc);

  // Convert the remaining operands to rvalues.
  for (std::size_t I = 1; I < Args.size(); ++I) {
    ExprResult Arg = DefaultLvalueConversion(Args[I]);
    if (Arg.isInvalid())
      return ExprError();
    Args[I] = Arg.get();
  }

  for (std::size_t I = 0; I < Args.size(); ++I) {
    Expr *E = Args[I];

    assert(!E->isTypeDependent() && !E->isValueDependent()
        && "Dependent element");

    // Get the canonical type of the expression.
    QualType T = E->getType();
    if (AutoType *D = T->getContainedAutoType()) {
      T = D->getDeducedType();
      if (!T.getTypePtr())
        llvm_unreachable("Undeduced value reflection");
    }
    T = Context.getCanonicalType(T);

    // Ensure we are either working with valid
    // operands.
    if (T->isConstantArrayType()) {
      QualType ElemTy = cast<ArrayType>(T.getTypePtr())->getElementType();
      if (ElemTy->isCharType())
        continue;
    }

    if (T->isIntegerType())
      continue;

    Diag(E->getExprLoc(), diag::err_reflect_print_literal_wrong_operand_type);
    return ExprError();
  }

  return new (Context) CXXReflectPrintLiteralExpr(
    Context, Context.IntTy, Args, KWLoc, LParenLoc, RParenLoc);
}

ExprResult Sema::ActOnCXXReflectPrintReflection(SourceLocation KWLoc,
                                                Expr *Reflection,
                                                SourceLocation LParenLoc,
                                                SourceLocation RParenLoc) {
  if (Reflection->isTypeDependent() || Reflection->isValueDependent())
    return new (Context) CXXReflectPrintReflectionExpr(
        Context, Context.DependentTy, Reflection, KWLoc, LParenLoc, RParenLoc);

  ExprResult Arg = DefaultLvalueConversion(Reflection);
  if (Arg.isInvalid())
    return ExprError();

  if (!CheckReflectionOperand(*this, Reflection))
    return ExprError();

  return new (Context) CXXReflectPrintReflectionExpr(
      Context, Context.IntTy, Arg.get(), KWLoc, LParenLoc, RParenLoc);
}

ExprResult Sema::ActOnCXXReflectDumpReflection(SourceLocation KWLoc,
                                               Expr *Reflection,
                                               SourceLocation LParenLoc,
                                               SourceLocation RParenLoc) {
  if (Reflection->isTypeDependent() || Reflection->isValueDependent())
    return new (Context) CXXReflectDumpReflectionExpr(
        Context, Context.DependentTy, Reflection, KWLoc, LParenLoc, RParenLoc);

  ExprResult Arg = DefaultLvalueConversion(Reflection);
  if (Arg.isInvalid())
    return ExprError();

  if (!CheckReflectionOperand(*this, Reflection))
    return ExprError();

  return new (Context) CXXReflectDumpReflectionExpr(
      Context, Context.IntTy, Arg.get(), KWLoc, LParenLoc, RParenLoc);
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

ExprResult Sema::ActOnCXXIdExprExpr(SourceLocation KWLoc,
                                    Expr *Refl,
                                    SourceLocation LParenLoc,
                                    SourceLocation RParenLoc)
{
  if (Refl->isTypeDependent() || Refl->isValueDependent())
    return new (Context) CXXIdExprExpr(Context.DependentTy, Refl, KWLoc,
                                       LParenLoc, LParenLoc);

  Reflection R = EvaluateReflection(*this, Refl);
  if (R.isInvalid())
    return ExprError();

  if (R.isExpression()) {
    if (const DeclRefExpr *Ref = dyn_cast<DeclRefExpr>(R.getAsExpression()))
      return const_cast<DeclRefExpr *>(Ref);
  }

  // FIXME: Emit a better error diagnostic.
  Diag(Refl->getExprLoc(), diag::err_expression_not_value_reflection);
  return ExprError();
}

ExprResult Sema::ActOnCXXValueOfExpr(SourceLocation KWLoc,
                                     Expr *Refl,
                                     SourceLocation LParenLoc,
                                     SourceLocation RParenLoc)
{
  if (Refl->isTypeDependent() || Refl->isValueDependent())
    return new (Context) CXXValueOfExpr(Context.DependentTy, Refl, KWLoc,
                                        LParenLoc, LParenLoc);

  Reflection R = EvaluateReflection(*this, Refl);
  if (R.isInvalid())
    return ExprError();

  Expr *Eval = nullptr;
  if (R.isDeclaration()) {
    // If this is a value declaration, the build a DeclRefExpr to evaluate.
    // Otherwise, it's not evaluable.
    if (const ValueDecl *VD = dyn_cast<ValueDecl>(R.getAsDeclaration())) {
      QualType T = VD->getType();
      ExprValueKind VK = Expr::getValueKindForType(T);
      Eval = BuildDeclRefExpr(const_cast<ValueDecl *>(VD), T, VK, KWLoc).get();
    }
  } else if (R.isExpression()) {
    // Just evaluate the expression.
    Eval = const_cast<Expr *>(R.getAsExpression());

    // But if the expression is a reference to a field. Adjust this to
    // be a pointer to that field.
    if (DeclRefExpr *Ref = dyn_cast<DeclRefExpr>(Eval)) {
      if (const FieldDecl *F = dyn_cast<FieldDecl>(Ref->getDecl())) {
        QualType Ty = F->getType();
        const Type *Cls = Context.getTagDeclType(F->getParent()).getTypePtr();
        Ty = Context.getMemberPointerType(Ty, Cls);
        Eval = new (Context) UnaryOperator(Ref, UO_AddrOf, Ty, VK_RValue, 
                                           OK_Ordinary, Ref->getExprLoc(), 
                                           false);
      }
    }
  }

  // Evaluate the resulting expression.
  SmallVector<PartialDiagnosticAt, 4> Diags;
  Expr::EvalResult Result;
  Result.Diag = &Diags;
  if (!Eval->EvaluateAsAnyValue(Result, Context)) {
    Diag(Eval->getExprLoc(), diag::reflection_not_constant_expression);
    for (PartialDiagnosticAt PD : Diags)
      Diag(PD.first, PD.second);
    return ExprError();
  }

  return new (Context) CXXConstantExpr(Eval, std::move(Result.Val));
}


static bool AppendStringValue(Sema& S, llvm::raw_ostream& OS,
                              const APValue& Val) {
  // Extracting the string value from the LValue.
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
BuildReflectedTypeTemplarg(Sema &S, SourceLocation KWLoc,
                           Expr *ReflExpr, const QualType &QT) {
  llvm::outs() << "Templarg - Type\n";
  const Type *T = QT.getTypePtr();
  void *OpaqueT = reinterpret_cast<void *>(const_cast<Type *>(T));
  return ParsedTemplateArgument(ParsedTemplateArgument::Type, OpaqueT, KWLoc);
}

static ParsedTemplateArgument
BuildReflectedDeclTemplarg(Sema &S, SourceLocation KWLoc,
                           Expr *ReflExpr, const Decl *D) {
  llvm::outs() << "Templarg - Decl\n";
  if (const TemplateDecl *TDecl = dyn_cast<TemplateDecl>(D)) {
    using TemplateTy = OpaquePtr<TemplateName>;
    TemplateTy Template = TemplateTy::make(
        TemplateName(const_cast<TemplateDecl *>(TDecl)));

    return ParsedTemplateArgument(ParsedTemplateArgument::Template,
                                  Template.getAsOpaquePtr(),
                                  KWLoc);
  }

  llvm_unreachable("Unsupported reflected declaration type");
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
    return BuildReflectedDeclTemplarg(*this, KWLoc, E, D);
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

ExprResult
Sema::ActOnDependentMemberExpr(Expr *BaseExpr, QualType BaseType,
                               SourceLocation OpLoc, bool IsArrow,
                               Expr *IdExpr) {
  // TODO The non-reflection variant of this provides diagnostics
  // for some situations even in the dependent context. This is
  // something we should consider implementing for the reflection
  // variant.
  assert(BaseType->isDependentType() ||
         IdExpr->isTypeDependent() ||
         IdExpr->isValueDependent());

  // Get the type being accessed in BaseType.  If this is an arrow, the BaseExpr
  // must have pointer type, and the accessed type is the pointee.
  return CXXDependentScopeMemberExpr::Create(Context, BaseExpr, BaseType,
                                             IsArrow, OpLoc, IdExpr);
}


ExprResult Sema::ActOnMemberAccessExpr(Expr *Base,
                                       SourceLocation OpLoc,
                                       tok::TokenKind OpKind,
                                       Expr *IdExpr) {
  bool IsArrow = (OpKind == tok::arrow);

  if (IdExpr->isTypeDependent() || IdExpr->isValueDependent()) {
    return ActOnDependentMemberExpr(Base, Base->getType(), OpLoc, IsArrow,
                                    IdExpr);
  }

  return BuildMemberReferenceExpr(Base, Base->getType(), OpLoc, IsArrow,
                                  IdExpr);
}

static MemberExpr *BuildMemberExpr(
    Sema &SemaRef, ASTContext &C, Expr *Base, bool isArrow,
    SourceLocation OpLoc, const ValueDecl *Member, QualType Ty, ExprValueKind VK,
    ExprObjectKind OK) {
  assert((!isArrow || Base->isRValue()) && "-> base must be a pointer rvalue");
  ValueDecl *ModMember = const_cast<ValueDecl *>(Member);

  // TODO: See if we can improve the source code location information here
  DeclarationNameInfo DeclNameInfo(Member->getDeclName(), SourceLocation());
  DeclAccessPair DeclAccessPair = DeclAccessPair::make(ModMember,
                                                       ModMember->getAccess());

  MemberExpr *E = MemberExpr::Create(
      C, Base, isArrow, OpLoc, /*QualifierLoc=*/NestedNameSpecifierLoc(),
      /*TemplateKWLoc=*/SourceLocation(), ModMember, DeclAccessPair,
      DeclNameInfo, /*TemplateArgs=*/nullptr, Ty, VK, OK);
  SemaRef.MarkMemberReferenced(E);
  return E;
}

// TODO there is a lot of logic that has been duplicated between
// this and the non-reflection variant of BuildMemberReferenceExpr,
// we should examine how we might be able to reduce said duplication.
ExprResult
Sema::BuildMemberReferenceExpr(Expr *BaseExpr, QualType BaseExprType,
                               SourceLocation OpLoc, bool IsArrow,
                               Expr *IdExpr) {
  assert(isa<DeclRefExpr>(IdExpr) && "must be a DeclRefExpr");

  // C++1z [expr.ref]p2:
  //   For the first option (dot) the first expression shall be a glvalue [...]
  if (!IsArrow && BaseExpr && BaseExpr->isRValue()) {
    ExprResult Converted = TemporaryMaterializationConversion(BaseExpr);
    if (Converted.isInvalid())
      return ExprError();
    BaseExpr = Converted.get();
  }

  const Decl *D = cast<DeclRefExpr>(IdExpr)->getDecl();

  if (const FieldDecl *FD = dyn_cast<FieldDecl>(D)) {
    // x.a is an l-value if 'a' has a reference type. Otherwise:
    // x.a is an l-value/x-value/pr-value if the base is (and note
    //   that *x is always an l-value), except that if the base isn't
    //   an ordinary object then we must have an rvalue.
    ExprValueKind VK = VK_LValue;
    ExprObjectKind OK = OK_Ordinary;
    if (!IsArrow) {
      if (BaseExpr->getObjectKind() == OK_Ordinary)
        VK = BaseExpr->getValueKind();
      else
        VK = VK_RValue;
    }
    if (VK != VK_RValue && FD->isBitField())
      OK = OK_BitField;

    // Figure out the type of the member; see C99 6.5.2.3p3, C++ [expr.ref]
    QualType MemberType = FD->getType();
    if (const ReferenceType *Ref = MemberType->getAs<ReferenceType>()) {
      MemberType = Ref->getPointeeType();
      VK = VK_LValue;
    } else {
      QualType BaseType = BaseExpr->getType();
      if (IsArrow) BaseType = BaseType->getAs<PointerType>()->getPointeeType();

      Qualifiers BaseQuals = BaseType.getQualifiers();

      // GC attributes are never picked up by members.
      BaseQuals.removeObjCGCAttr();

      // CVR attributes from the base are picked up by members,
      // except that 'mutable' members don't pick up 'const'.
      if (FD->isMutable()) BaseQuals.removeConst();

      Qualifiers MemberQuals =
          Context.getCanonicalType(MemberType).getQualifiers();

      assert(!MemberQuals.hasAddressSpace());

      Qualifiers Combined = BaseQuals + MemberQuals;
      if (Combined != MemberQuals)
        MemberType = Context.getQualifiedType(MemberType, Combined);
    }

    return BuildMemberExpr(*this, Context, BaseExpr, IsArrow, OpLoc,
                           FD, MemberType, VK, OK);
  }

  if (const CXXMethodDecl *MemberFn = dyn_cast<CXXMethodDecl>(D)) {
    QualType MemberTy;
    ExprValueKind VK;
    if (MemberFn->isInstance()) {
      MemberTy = Context.BoundMemberTy;
      VK = VK_RValue;
    } else {
      MemberTy = MemberFn->getType();
      VK = VK_LValue;
    }

    return BuildMemberExpr(*this, Context, BaseExpr, IsArrow, OpLoc,
                           MemberFn, MemberTy, VK, OK_Ordinary);
  }

  llvm_unreachable("Unsupported decl type");
}

