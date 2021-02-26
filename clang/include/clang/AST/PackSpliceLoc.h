//===- PackSplice.h - C++ Pack Splice Representation ------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines the PackSpliceLoc class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_PACKSPLICELOC_H
#define LLVM_CLANG_AST_PACKSPLICELOC_H

#include "clang/Basic/SourceLocation.h"

namespace clang {

class PackSplice;

// A representation of a PackSplice with addition source location
// information.
class PackSpliceLoc {
  const PackSplice *PS;

  SourceLocation EllipsisLoc;
  SourceLocation SBELoc;
  SourceLocation SEELoc;
public:
  PackSpliceLoc(const PackSplice *PS, SourceLocation EllipsisLoc,
                SourceLocation SBELoc, SourceLocation SEELoc)
      : PS(PS), EllipsisLoc(EllipsisLoc), SBELoc(SBELoc), SEELoc(SEELoc) { }

  const PackSplice *getPackSplice() const { return PS; }

  SourceLocation getEllipsisLoc() const { return EllipsisLoc; }
  SourceLocation getSBELoc() const { return SBELoc; }
  SourceLocation getSEELoc() const { return SEELoc; }

  SourceLocation getBeginLoc() const { return EllipsisLoc; }
  SourceLocation getEndLoc() const { return SEELoc; }

  SourceRange getSourceRange() const {
    return SourceRange(getBeginLoc(), getEndLoc());
  }
};

} // namespace clang

#endif // LLVM_CLANG_AST_PACKSPLICELOC_H
