#ifndef LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMETYPECATEGORY_H
#define LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMETYPECATEGORY_H

#include "clang/AST/Type.h"
#include "clang/Analysis/Analyses/Lifetime.h"

namespace clang {
namespace lifetime {
TypeCategory classifyTypeCategory(QualType QT);
}
} // namespace clang

#endif // LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMETYPECATEGORY_H