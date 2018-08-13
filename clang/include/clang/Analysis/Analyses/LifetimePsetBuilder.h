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

#include "LifetimePset.h"
#include "clang/Basic/SourceLocation.h"

namespace clang {
class CFGBlock;
class ASTContext;

namespace lifetime {
class LifetimeReporterBase;

/// Updates psets with all effects that appear in the block.
/// \param Reporter if non-null, emits diagnostics
void VisitBlock(PSetsMap &PMap, llvm::Optional<PSetsMap> &FalseBranchExitPMap,
                std::map<const Expr *, PSet> &PSetsOfExpr,
                std::map<const Expr *, PSet> &RefersTo, const CFGBlock &B,
                LifetimeReporterBase &Reporter, ASTContext &ASTCtxt);

/// Get the initial PSets for function parameters.
void PopulatePSetForParams(PSetsMap &PMap, const FunctionDecl *FD);
} // namespace lifetime
} // namespace clang

#endif // LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMEPSETBUILDER_H
