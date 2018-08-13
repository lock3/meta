//=- Lifetime.h - Diagnose lifetime violations -*- C++ -*-====================//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIME_H
#define LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIME_H

#include "clang/Basic/SourceLocation.h"
#include <string>

namespace clang {
class FunctionDecl;
class ASTContext;
class SourceManager;
class VarDecl;
class Sema;

namespace lifetime {
enum class TypeCategory { Owner, Pointer, Aggregate, Value };

class LifetimeReporterBase {
public:
  virtual ~LifetimeReporterBase() = default;
  virtual void warnPsetOfGlobal(SourceLocation Loc, StringRef VariableName,
                                std::string ActualPset) const = 0;
  virtual void warnDerefDangling(SourceLocation Loc, bool possibly) const = 0;
  virtual void warnDerefNull(SourceLocation Loc, bool possibly) const = 0;
  virtual void warnParametersAlias(SourceLocation LocParam1,
                                   SourceLocation LocParam2,
                                   const std::string &Pointee) const = 0;
  virtual void warnParameterDangling(SourceLocation Loc,
                                     bool indirectly) const = 0;
  virtual void warnParameterNull(SourceLocation Loc, bool possibly) const = 0;
  virtual void notePointeeLeftScope(SourceLocation Loc,
                                    std::string Name) const = 0;

  virtual void debugPset(SourceLocation Loc, StringRef Variable,
                         std::string Pset) const = 0;
  virtual void debugTypeCategory(SourceLocation Loc,
                                 TypeCategory Category) const = 0;

  // TODO: remove me
  virtual void diag(SourceLocation Loc, unsigned DiagID) const = 0;
};

void runAnalysis(const FunctionDecl *Func, ASTContext &Context,
                 LifetimeReporterBase &Reporter);
} // namespace lifetime
} // namespace clang

#endif // LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIME_H
