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

#include "clang/Basic/OperatorKinds.h"
#include "clang/Basic/SourceLocation.h"
#include <string>

namespace clang {
class FunctionDecl;
class ASTContext;
class SourceManager;
class VarDecl;
class Sema;
class QualType;
class ClassTemplateSpecializationDecl;
class CXXRecordDecl;
class FunctionDecl;

namespace lifetime {
enum class TypeCategory { Owner, Pointer, Aggregate, Value };

using LookupOperatorTy = llvm::function_ref<FunctionDecl *(
    const CXXRecordDecl *R, OverloadedOperatorKind Op)>;
extern LookupOperatorTy GlobalLookupOperator;

using LookupMemberFunctionTy =
    llvm::function_ref<FunctionDecl *(const CXXRecordDecl *R, StringRef Name)>;
extern LookupMemberFunctionTy GlobalLookupMemberFunction;

using DefineClassTemplateSpecializationTy =
    llvm::function_ref<void(ClassTemplateSpecializationDecl *Specialization)>;
extern DefineClassTemplateSpecializationTy
    GlobalDefineClassTemplateSpecialization;

using IsConvertibleTy = llvm::function_ref<bool(QualType, QualType)>;

enum class WarnType {
  DerefDangling,
  DerefNull,
  AssignNull,
  ParamNull,
  ReturnDangling,
  ReturnNull
};

enum class NoteType {
  NeverInit,
  TempDestroyed,
  Dereferenced,
  ForbiddenCast,
  Modified,
  Deleted,
  Assigned,
  ParamNull,
  NullDefaultConstructed,
  ComparedToNull,
  NullConstant,
  PointeeLeftScope
};

class LifetimeReporterBase {
public:
  virtual ~LifetimeReporterBase() = default;
  virtual void warnPsetOfGlobal(SourceLocation Loc, StringRef VariableName,
                                std::string ActualPset) = 0;
  virtual void warn(WarnType T, SourceRange Range, bool Possibly) = 0;
  virtual void warnParameterDangling(SourceRange Range, bool Indirectly) = 0;
  virtual void warnReturnWrongPset(SourceRange Range, StringRef RetPset,
                                   StringRef ExpectedPset) = 0;
  virtual void warnPointerArithmetic(SourceRange Range) = 0;
  virtual void warnNonStaticThrow(SourceRange Range, StringRef ThrownPset) = 0;
  virtual void notePointeeLeftScope(SourceRange Range, std::string Name) = 0;
  virtual void note(NoteType T, SourceRange Range) = 0;
  virtual void debugPset(SourceRange Range, StringRef Variable,
                         std::string Pset) = 0;
  virtual void debugTypeCategory(SourceRange Range, TypeCategory Category,
                                 StringRef Pointee = "") = 0;
};

void runAnalysis(
    const FunctionDecl *Func, ASTContext &Context,
    LifetimeReporterBase &Reporter, IsConvertibleTy IsConvertible,
    LookupOperatorTy LookupOperator,
    LookupMemberFunctionTy LookupMemberFunction,
    DefineClassTemplateSpecializationTy DefineClassTemplateSpecialization);
} // namespace lifetime
} // namespace clang

#endif // LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIME_H
