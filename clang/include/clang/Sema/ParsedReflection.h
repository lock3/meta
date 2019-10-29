//===--- ParsedReflection.h - Template Parsing Data Types -------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file provides data structures that store the parsed representation of
//  operands to the reflexpr expression.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_SEMA_PARSEDREFLECTION_H
#define LLVM_CLANG_SEMA_PARSEDREFLECTION_H

#include "clang/Basic/OperatorKinds.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/TemplateKinds.h"
#include "clang/Sema/DeclSpec.h"
#include "clang/Sema/Ownership.h"
#include "llvm/ADT/SmallVector.h"
#include <cassert>
#include <cstdlib>
#include <new>

namespace clang {
  using ReflectedNamespace =
    llvm::PointerUnion<NamespaceDecl *, TranslationUnitDecl *>;

  /// Represents the parsed operand of a reflexpr expression. Note that this
  /// is essentially the same as a ParsedTemplateArgument, except that it
  /// also allows namespace-names, but not pack expansions.
  class ParsedReflectionOperand {
  public:
    /// Describes the kind of operand that was parsed.
    enum KindType {
      /// An invalid reflection operand.
      Invalid,
      /// A reflection of a type-id.
      Type,
      /// A reflection of either a template-name.
      Template,
      /// A reflection of a namespace-name.
      Namespace,
      /// A reflection of the global namespace.
      GlobalNamespace,
      /// A reflection of an expression.
      Expression,
    };

    /// Build an invalid reflection operand.
    ParsedReflectionOperand() : Kind(Invalid), Operand(nullptr) { }

    /// Create a type argument or expression operand.
    ParsedReflectionOperand(ExprResult E, SourceLocation Loc)
      : Kind(Expression), Operand(E.get()), Loc(Loc) { }

    /// Create a type argument or expression operand.
    ParsedReflectionOperand(ParsedType T, SourceLocation Loc)
      : Kind(Type), Operand(T.getAsOpaquePtr()), Loc(Loc) { }

    /// Create a template operand.
    ParsedReflectionOperand(const CXXScopeSpec &SS,
                            ParsedTemplateTy Temp,
                            SourceLocation Loc)
      : Kind(Template), Operand(Temp.getAsOpaquePtr()), SS(SS), Loc(Loc) { }

    /// Create a namespace operand.
    ParsedReflectionOperand(const CXXScopeSpec &SS,
                            Decl* Ns,
                            SourceLocation Loc)
      : Kind(Namespace), Operand(Ns), SS(SS), Loc(Loc) { }

    /// Create a global namespace operand.
    ParsedReflectionOperand(TranslationUnitDecl *Ns,
                            SourceLocation Loc)
      : Kind(GlobalNamespace), Operand(Ns), SS(), Loc(Loc) { }


    /// Determine whether this operand is invalid.
    bool isInvalid() const { return Kind == Invalid; }

    /// Determine what kind of template argument we have.
    KindType getKind() const { return Kind; }

    /// Retrieve the template type argument's type.
    ParsedType getAsType() const {
      assert(Kind == Type && "Not a type");
      return ParsedType::getFromOpaquePtr(Operand);
    }

    /// Retrieve the non-type template argument's expression.
    Expr *getAsExpr() const {
      assert(Kind == Expression && "Not an expression");
      return static_cast<Expr*>(Operand);
    }

    /// Retrieve the template template argument's template name.
    ParsedTemplateTy getAsTemplate() const {
      assert(Kind == Template && "Not a template");
      return ParsedTemplateTy::getFromOpaquePtr(Operand);
    }

    /// Retrieve the template template argument's template name.
    ReflectedNamespace getAsNamespace() const {
      assert((Kind == Namespace || Kind == GlobalNamespace)
             && "Not a namespace");
      if (Kind == Namespace)
        return static_cast<NamespaceDecl *>(Operand);
      else
        return static_cast<TranslationUnitDecl *>(Operand);
    }

    /// Retrieve the location of the template argument.
    SourceLocation getLocation() const { return Loc; }

    /// Retrieve the nested-name-specifier that precedes the template
    /// name in a template template argument.
    const CXXScopeSpec &getScopeSpec() const {
      assert((Kind == Template || Kind == Namespace || Kind == GlobalNamespace) &&
             "Only template template arguments can have a scope specifier");
      return SS;
    }

  private:
    /// The kind of parsed argument.
    KindType Kind;

    /// The actual template argument representation, which may be
    /// an \c Sema::TypeTy* (for a type), an Expr* (for an
    /// expression), an Sema::TemplateTy (for a template), or a
    /// Decl* (for a namespace-name).
    ///
    void *Operand;

    /// The nested-name-specifier that can accompany a reflected
    /// template-name or namespace-name.
    CXXScopeSpec SS;

    /// the location of the template argument.
    SourceLocation Loc;
  };

} // end namespace clang

#endif // LLVM_CLANG_SEMA_PARSEDREFLECTION_H
