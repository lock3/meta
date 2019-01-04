//===- NestedNameSpecifier.h - C++ nested name specifiers -------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the NestedNameSpecifier class, which represents
//  a C++ nested-name-specifier.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_CXXINJECTIONCONTEXTSPECIFIER_H
#define LLVM_CLANG_AST_CXXINJECTIONCONTEXTSPECIFIER_H

#include "clang/Basic/SourceLocation.h"

namespace clang {

class CXXInjectionContextSpecifier {
public:
  enum Kind {
    CurrentContext,
    ParentNamespace,
    SpecifiedNamespace
  };

  // Constructs a default injection context specifier, injecting
  // into current context -- as determined at constexpr evaluation
  // time.
  CXXInjectionContextSpecifier()
    : ContextKind(CurrentContext), NSDecl(nullptr),
      BeginLoc(), EndLoc() { }

  // Constructs an injection context specifier, injecting
  // into the specified namespace.
  CXXInjectionContextSpecifier(SourceLocation BeginLoc,
                               Decl *NSDecl, SourceLocation EndLoc)
    : ContextKind(SpecifiedNamespace), NSDecl(NSDecl),
      BeginLoc(BeginLoc), EndLoc(EndLoc) { }

  // Constructs an injection context specifier, injecting
  // into a specified context -- determined at constexpr evaluation
  // time.
  CXXInjectionContextSpecifier(SourceLocation KWLoc, Kind ContextKind)
    : ContextKind(ContextKind), NSDecl(nullptr),
      BeginLoc(KWLoc), EndLoc(KWLoc) {
    assert((ContextKind == ParentNamespace)
           && "Invalid context kind for this constructor");
  }

  Kind getContextKind() const {
    return ContextKind;
  }

  Decl *getSpecifiedNamespace() const {
    assert(ContextKind == SpecifiedNamespace);
    return NSDecl;
  }

  SourceLocation getBeginLoc() const {
    return BeginLoc;
  }

  SourceLocation getEndLoc() const {
    return EndLoc;
  }

private:
  Kind ContextKind;
  Decl *NSDecl;

  SourceLocation BeginLoc;
  SourceLocation EndLoc;
};

} // namespace clang

#endif // LLVM_CLANG_AST_CXXINJECTIONCONTEXTSPECIFIER_H
