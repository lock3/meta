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
#include "clang/Sema/Scope.h"
#include "clang/Sema/SemaInternal.h"
#include "TypeLocBuilder.h"
using namespace clang;
using namespace clang::reflect;
using namespace sema;

/// Lookup the declaration named by SS and Id. Populates the the kind
/// and entity with the encoded reflection of the named entity.
bool Sema::ActOnReflectedId(CXXScopeSpec &SS, SourceLocation IdLoc,
                            IdentifierInfo *Id, unsigned &Kind,
                            ParsedReflectionPtr &Entity) {
  // Perform any declaration having the given name.
  LookupResult R(*this, Id, IdLoc, LookupAnyName);
  LookupParsedName(R, CurScope, &SS);

  if (!R.isSingleResult()) {
    // FIXME: Incorporate the scope specifier in the diagnostics. Also note
    // alternatives for an ambiguous lookup.
    //
    // FIXME: I believe that we would eventually like to support overloaded
    // declarations and templates.
    if (R.isAmbiguous())
      Diag(IdLoc, diag::err_reflect_ambiguous_id) << Id;
    else if (R.isOverloadedResult())
      Diag(IdLoc, diag::err_reflect_overloaded_id) << Id;
    else
      Diag(IdLoc, diag::err_reflect_undeclared_id) << Id;
    Kind = REK_special;
    Entity = nullptr;
    return false;
  }

  Decl *D = R.getAsSingle<NamedDecl>();

  if (const ValueDecl *VD = dyn_cast<ValueDecl>(D)) {
    DeclRefExpr *Ref = new (Context) DeclRefExpr(
      const_cast<ValueDecl*>(VD), false, VD->getType(),
      VK_RValue, IdLoc);
    Entity = Ref;
    Kind = REK_statement;
  } else if (const TypeDecl *TD = dyn_cast<TypeDecl>(D)) {
    QualType T = Context.getTypeDeclType(TD);

    // FIXME: There are other types that can be declarations.
    if (const TagDecl *Tag = T->getAsTagDecl()) {
      Entity = Tag;
      Kind = REK_declaration;
    } else if (const BuiltinType *BT = T->getAs<BuiltinType>()) {
      // As builtin types are builtin, there is not an available
      // decl to use for reflection, thus this must be a type reflection.
      Entity = BT;
      Kind = REK_type;
    } else {
      Entity = T.getTypePtr();
      Kind = REK_type;
    }
  } else {
    // This is something like a namespace. Just use TransformDecl even
    // though it's unlikely to change.
    Entity = D;
    Kind = REK_declaration;
  }

  return true;
}

bool Sema::ActOnReflectedDependentId(CXXScopeSpec &SS, SourceLocation IdLoc,
                                     IdentifierInfo *Id, unsigned &Kind,
                                     ParsedReflectionPtr &Entity) {
  LookupResult R(*this, Id, IdLoc, LookupAnyName);
  LookupParsedName(R, CurScope, &SS, false, true);
  DeclarationNameInfo DNI = R.getLookupNameInfo();

  if (!R.isSingleResult()) {
    // FIXME: Incorporate the scope specifier in the diagnostics. Also note
    // alternatives for an ambiguous lookup.
    //
    // FIXME: I believe that we would eventually like to support overloaded
    // declarations and templates.
    if (R.isAmbiguous())
      Diag(IdLoc, diag::err_reflect_ambiguous_id) << Id;
    else if (R.isOverloadedResult())
      Diag(IdLoc, diag::err_reflect_overloaded_id) << Id;
    else
      Diag(IdLoc, diag::err_reflect_undeclared_id) << Id;
    Kind = REK_special;
    Entity = nullptr;
    return false;
  }

  const auto &decls = R.asUnresolvedSet();

  Entity =
    UnresolvedLookupExpr::Create(Context, /*NamingClass=*/ nullptr,
                                 SS.getWithLocInContext(Context),
                                 DNI, /*ADL=*/false, /*Overloaded=*/false,
                                 decls.begin(), decls.end());
  Kind = REK_statement;
  return true;
}

/// Populates the kind and entity with the encoded reflection of the type.
/// Reflections of user-defined types are handled as entities.
bool Sema::ActOnReflectedType(Declarator &D, unsigned &Kind,
                              ParsedReflectionPtr &Entity) {
  TypeSourceInfo *TSI = GetTypeForDeclarator(D, CurScope);
  QualType T = TSI->getType();
  // FIXME: There are other types that can be declarations.
  if (TagDecl *TD = T->getAsTagDecl()) {
    // Handle elaborated type specifiers as if they were declarations.
    Entity = TD;
    Kind = REK_declaration;
  } else {
    // Otherwise, this is a type reflection.
    Entity = const_cast<Type*>(T.getTypePtr());
    Kind = REK_type;
  }
  return true;
}

/// Returns a constant expression that encodes the value of the reflection.
/// The type of the reflection is meta::reflection, an enum class.
ExprResult Sema::ActOnCXXReflectExpression(SourceLocation KWLoc,
                                           unsigned Kind,
                                           ParsedReflectionPtr Entity,
                                           SourceLocation LPLoc,
                                           SourceLocation RPLoc) {
  ReflectionKind REK = (ReflectionKind)Kind;
  APValue Reflection(REK, Entity);

  bool IsValueDependent = false;
  if (const Type* T = getAsReflectedType(Reflection)) {
    // A type reflection is dependent if reflected T is dependent.
    IsValueDependent = T->isDependentType();
  } else if (const Decl *D = getAsReflectedDeclaration(Reflection)) {
    if (const ValueDecl *VD = dyn_cast<ValueDecl>(D)) {
      // A declaration reflection is value dependent if an id-expression
      // referring to that declaration is type or value dependent. Reflections
      // of non-value declarations (e.g., namespaces) are never dependent.
      // Build a fake id-expression in order to determine dependence.
      Expr *E = new (Context) DeclRefExpr(const_cast<ValueDecl*>(VD), false,
                                          VD->getType(), VK_RValue, KWLoc);
      IsValueDependent = E->isTypeDependent() || E->isValueDependent();
    } else if (const TypeDecl *TD = dyn_cast<TypeDecl>(D)) {
      // A reflection of a type declaration is dependent if that type is
      // dependent.
      const Type* T = TD->getTypeForDecl();
      IsValueDependent = T->isDependentType();
    }
  } else if (const UnresolvedLookupExpr *ULE = getAsReflectedULE(Reflection)) {
    IsValueDependent = true;
  }

  return CXXReflectExpr::Create(Context, KWLoc, Context.MetaInfoTy, Reflection,
                                LPLoc, RPLoc, VK_RValue,
                                /*isTypeDependent=*/false,
                                /*isValueDependent=*/IsValueDependent,
                                /*isInstDependent=*/IsValueDependent,
                                /*containsUnexpandedPacks=*/false);
}

// Convert each operand to an rvalue.
static void ConvertTraitOperands(Sema &SemaRef, ArrayRef<Expr *> Args,
                               SmallVectorImpl<Expr *> &Operands) {
  for (std::size_t I = 0; I < Args.size(); ++I) {
    if (Args[I]->isGLValue())
      Operands[I] = ImplicitCastExpr::Create(SemaRef.Context,
                                             Args[I]->getType(),
                                             CK_LValueToRValue, Args[I],
                                             nullptr, VK_RValue);
    else
      Operands[I] = Args[I];
  }
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
    SemaRef.Diag(E->getBeginLoc(), diag::err_reflection_trait_wrong_type)
        << Source;
    return false;
  }

  return true;
}

ExprResult Sema::ActOnCXXReflectionTrait(SourceLocation TraitLoc,
                                         ReflectionTrait Trait,
                                         ArrayRef<Expr *> Args,
                                         SourceLocation RParenLoc) {
  // If any arguments are dependent, then the expression is dependent.
  for (std::size_t I = 0; I < Args.size(); ++I) {
    Expr *Arg = Args[0];
    if (Arg->isTypeDependent() || Arg->isValueDependent())
      return new (Context) CXXReflectionTraitExpr(Context, Context.DependentTy,
                                                  Trait, TraitLoc, Args,
                                                  RParenLoc);
  }

  // Build a set of converted arguments.
  SmallVector<Expr *, 2> Operands(Args.size());
  ConvertTraitOperands(*this, Args, Operands);

  // Check the type of the first operand. Note: ReflectPrint is polymorphic.
  if (Trait != URT_ReflectPrint) {
    if (!CheckReflectionOperand(*this, Operands[0]))
      return false;
  }

  // FIXME: If the trait allows multiple arguments, check those.
  QualType ResultTy;
  switch (Trait) {
    case URT_ReflectIndex: // meta::reflection_kind
      ResultTy = Context.IntTy;
      break;

    case URT_ReflectContext:
    case URT_ReflectHome:
    case URT_ReflectBegin:
    case URT_ReflectEnd:
    case URT_ReflectNext:
    case URT_ReflectType: // meta::info
      ResultTy = Context.MetaInfoTy;
      break;

    case URT_ReflectName: // const char*
      ResultTy = Context.getPointerType(Context.CharTy.withConst());
      break;

    case URT_ReflectTraits: // unsigned
      ResultTy = Context.UnsignedIntTy;
      break;

    case URT_ReflectPrint: // int (always 0)
      // Returns 0.
      ResultTy = Context.IntTy;
      break;
  }
  assert(!ResultTy.isNull() && "unknown reflection trait");

  return new (Context) CXXReflectionTraitExpr(Context, ResultTy, Trait,
                                              TraitLoc, Operands, RParenLoc);
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
                    const Type *T) {
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

static void EvaluateReflection(Sema &S, Expr *E, APValue &Reflection, bool &Failed) {
  SmallVector<PartialDiagnosticAt, 4> Diags;
  Expr::EvalResult Result;
  Result.Diag = &Diags;
  if (!E->EvaluateAsRValue(Result, S.Context)) {
    S.Diag(E->getExprLoc(), diag::reflection_not_constant_expression);
    for (PartialDiagnosticAt PD : Diags)
      S.Diag(PD.first, PD.second);
    Failed = true;
    return;
  }

  Reflection = Result.Val;
  if (isNullReflection(Reflection)) {
    S.Diag(E->getExprLoc(), diag::err_empty_type_reflection);
    Failed = true;
  }
}

static bool
AppendReflection(Sema& S, llvm::raw_ostream &OS, Expr *E) {
  APValue Reflection;
  bool EvalFailed;
  EvaluateReflection(S, E, Reflection, EvalFailed);

  if (EvalFailed)
    return false;

  if (const Decl *D = getAsReflectedDeclaration(Reflection))
    return AppendReflectedDecl(S, OS, E, D);

  if (const Type *T = getAsReflectedType(Reflection))
    return AppendReflectedType(S, OS, E, T);

  if (const Expr *E = getAsReflectedStatement(Reflection))
    if (const DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(E))
      return AppendReflectedDecl(S, OS, E, DRE->getDecl());

  llvm_unreachable("Unsupported reflection type");
}

static bool HasDependentParts(SmallVectorImpl<Expr *>& Parts) {
 return std::any_of(Parts.begin(), Parts.end(), [](const Expr *E) {
    return E->isTypeDependent();
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
                           SourceLocation LParenLoc,
                           SourceLocation RParenLoc) {
  DeclarationNameInfo NameInfo = BuildReflectedIdName(LParenLoc, Parts, RParenLoc);
  DeclarationName Name = NameInfo.getName();
  if (!Name)
    return true;
  if (Name.getNameKind() == DeclarationName::CXXReflectedIdName)
    Result.setReflectedId(LParenLoc, Name.getCXXReflectedIdArguments(),
                          RParenLoc);
  else
    Result.setIdentifier(Name.getAsIdentifierInfo(), LParenLoc);
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
  if (!CheckReflectionOperand(*this, E))
    return ExprError();

  // TODO: Process the reflection E, into DeclRefExpr

  // CXXUnreflexprExpr *Val =
  //     new (Context) CXXUnreflexprExpr(E, E->getType(), E->getValueKind(),
  //                                     E->getObjectKind(), Loc);

  // return Val;
  return ExprError();
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

/// Evaluates the given expression and yields the computed type.
QualType Sema::BuildReflectedType(SourceLocation TypenameLoc, Expr *E) {
  if (E->isTypeDependent() || E->isValueDependent())
    return Context.getReflectedType(E, Context.DependentTy);

  // The operand must be a reflection.
  if (!CheckReflectionOperand(*this, E))
    return QualType();

  APValue Reflection;
  bool EvalFailed;
  EvaluateReflection(*this, E, Reflection, EvalFailed);

  if (EvalFailed) {
    return QualType();
  }

  // Get the type of the reflected entity.
  QualType Reflected;
  if (const Type* T = getAsReflectedType(Reflection)) {
    Reflected = QualType(T, 0);
  } else if (const Decl* D = getAsReflectedDeclaration(Reflection)) {
    if (const TypeDecl *TD = dyn_cast<TypeDecl>(D)) {
      Reflected = Context.getTypeDeclType(TD);
    } else {
      Diag(E->getExprLoc(), diag::err_expression_not_type_reflection);
      return QualType();
    }
  } else {
    // FIXME: Handle things like base classes.
    llvm_unreachable("unknown reflection");
  }

  return Context.getReflectedType(E, Reflected);
}


