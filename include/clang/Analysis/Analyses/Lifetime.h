//=- Lifetime.h - Diagnose lifetime violations -*- C++ -*-=//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines APIs for invoking and reported uninitialized values
// warnings.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIME_H
#define LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIME_H

#include "clang/Basic/SourceLocation.h"
#include <string>

namespace clang {

enum class TypeCategory { Owner, Pointer, Aggregate, Value };

class FunctionDecl;
class ASTContext;
class SourceManager;
class VarDecl;
class Sema;

class LifetimeReporterBase {
public:
  virtual ~LifetimeReporterBase() = default;
  virtual void warnPsetOfGlobal(SourceLocation Loc, StringRef VariableName,
                                std::string ActualPset) const = 0;
  virtual void warnDerefDangling(SourceLocation Loc, bool possibly) const = 0;
  virtual void warnDerefNull(SourceLocation Loc, bool possibly) const = 0;

  virtual void notePointeeLeftScope(SourceLocation Loc,
                                    std::string Name) const = 0;

  virtual void debugPset(SourceLocation Loc, StringRef Variable,
                         std::string Pset) const = 0;
  virtual void debugTypeCategory(SourceLocation Loc,
                                 TypeCategory Category) const = 0;

  // TODO: remove me
  virtual void diag(SourceLocation Loc, unsigned DiagID) const = 0;
};

void runLifetimeAnalysis(const FunctionDecl *Func, ASTContext &Context,
                         SourceManager &SourceMgr,
                         LifetimeReporterBase &Reporter);
void runLifetimeAnalysis(const VarDecl *V, ASTContext &Context,
                         LifetimeReporterBase &Reporter);

} // namespace clang

#endif // LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIME_H
