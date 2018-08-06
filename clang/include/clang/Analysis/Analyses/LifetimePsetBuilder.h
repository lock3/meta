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
class Stmt;
class VarDecl;

namespace lifetime {
class LifetimeReporterBase;

/// Updates psets with all effects that appear in the block.
/// \param Reporter if non-null, emits diagnostics
void VisitBlock(PSetsMap &PSets, const CFGBlock &B,
                const LifetimeReporterBase *Reporter, ASTContext &ASTCtxt);

/// Updates psets to remove 'null' when entering conditional statements. If
/// 'positive' is false, handles expression as-if it was negated.
/// Examples:
///   int* p = f();
/// if(p)
///  ... // pset of p does not contain 'null'
/// else
///  ... // pset of p is 'null'
/// if(!p)
///  ... // pset of p is 'null'
/// else
///  ... // pset of p does not contain 'null'
/// \param Reporter if non-null, emits diagnostics
void UpdatePSetsFromCondition(PSetsMap &PSets,
                              const LifetimeReporterBase *Reporter,
                              ASTContext &ASTCtxt, const Stmt *S, bool Positive,
                              SourceLocation Loc);

/// Evaluate a variable declartation for effects on psets
/// \param Reporter if non-null, emits diagnostics
void EvalVarDecl(PSetsMap &PSets, const VarDecl *VD,
                 const LifetimeReporterBase *Reporter, ASTContext &ASTCtxt);
} // namespace lifetime
} // namespace clang

#endif // LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMEPSETBUILDER_H
