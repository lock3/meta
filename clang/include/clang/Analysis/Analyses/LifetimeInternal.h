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

#ifndef LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMEINTERNAL_H
#define LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMEINTERNAL_H

#include "Lifetime.h"
#include <string>

namespace clang {
namespace lifetime {
  TypeCategory classifyTypeCategory(QualType QT);
}
} // namespace clang

#endif // LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMEINTERNAL_H
