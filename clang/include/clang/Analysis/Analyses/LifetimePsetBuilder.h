//=- LifetimePsetBuilder.h - Diagnose lifetime violations -*- C++ -*-=========//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMEPSETBUILDER_H
#define LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMEPSETBUILDER_H

#include "Lifetime.h"
#include "LifetimePset.h"
#include "clang/Basic/SourceLocation.h"

namespace clang {
class CFGBlock;
class ASTContext;

namespace lifetime {
class LifetimeReporterBase;

/// Updates psets with all effects that appear in the block.
/// \param Reporter if non-null, emits diagnostics
/// \returns false when an unsupported AST node disabled the analysis
bool VisitBlock(const FunctionDecl *FD, PSetsMap &PMap,
                llvm::Optional<PSetsMap> &FalseBranchExitPMap,
                std::map<const Expr *, PSet> &PSetsOfExpr,
                std::map<const Expr *, PSet> &RefersTo, const CFGBlock &B,
                LifetimeReporterBase &Reporter, ASTContext &ASTCtxt,
                IsConvertibleTy IsConvertible);

/// Get the initial PSets for function parameters.
void getLifetimeContracts(PSetsMap &PMap, const FunctionDecl *FD,
                          const ASTContext &ASTCtxt, const CFGBlock *Block,
                          IsConvertibleTy isConvertible,
                          LifetimeReporterBase &Reporter, bool Pre = true);
} // namespace lifetime
} // namespace clang

#endif // LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMEPSETBUILDER_H
