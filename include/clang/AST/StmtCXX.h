//===--- StmtCXX.h - Classes for representing C++ statements ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the C++ statement AST node classes.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_STMTCXX_H
#define LLVM_CLANG_AST_STMTCXX_H

#include "clang/AST/DeclarationName.h"
#include "clang/AST/Expr.h"
#include "clang/AST/NestedNameSpecifier.h"
#include "clang/AST/Stmt.h"
#include "llvm/Support/Compiler.h"

namespace clang {

class VarDecl;
class CXXMethodDecl;
class CXXRecordDecl;
class TemplateParameterList;
class NonTypeTemplateParmDecl;

/// CXXCatchStmt - This represents a C++ catch block.
///
class CXXCatchStmt : public Stmt {
  SourceLocation CatchLoc;
  /// The exception-declaration of the type.
  VarDecl *ExceptionDecl;
  /// The handler block.
  Stmt *HandlerBlock;
  
public:
  CXXCatchStmt(SourceLocation catchLoc, VarDecl *exDecl, Stmt *handlerBlock)
  : Stmt(CXXCatchStmtClass), CatchLoc(catchLoc), ExceptionDecl(exDecl),
    HandlerBlock(handlerBlock) {}

  CXXCatchStmt(EmptyShell Empty)
  : Stmt(CXXCatchStmtClass), ExceptionDecl(nullptr), HandlerBlock(nullptr) {}

  LLVM_ATTRIBUTE_DEPRECATED(SourceLocation getLocStart() const LLVM_READONLY,
                            "Use getBeginLoc instead") {
    return getBeginLoc();
  }
  SourceLocation getBeginLoc() const LLVM_READONLY { return CatchLoc; }
  LLVM_ATTRIBUTE_DEPRECATED(SourceLocation getLocEnd() const LLVM_READONLY,
                            "Use getEndLoc instead") {
    return getEndLoc();
  }
  SourceLocation getEndLoc() const LLVM_READONLY {
    return HandlerBlock->getEndLoc();
  }

  SourceLocation getCatchLoc() const { return CatchLoc; }
  VarDecl *getExceptionDecl() const { return ExceptionDecl; }
  QualType getCaughtType() const;
  Stmt *getHandlerBlock() const { return HandlerBlock; }

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == CXXCatchStmtClass;
  }

  child_range children() { return child_range(&HandlerBlock, &HandlerBlock+1); }

  friend class ASTStmtReader;
};

/// CXXTryStmt - A C++ try block, including all handlers.
///
class CXXTryStmt final : public Stmt,
                         private llvm::TrailingObjects<CXXTryStmt, Stmt *> {

  friend TrailingObjects;
  friend class ASTStmtReader;

  SourceLocation TryLoc;
  unsigned NumHandlers;
  size_t numTrailingObjects(OverloadToken<Stmt *>) const { return NumHandlers; }

  CXXTryStmt(SourceLocation tryLoc, Stmt *tryBlock, ArrayRef<Stmt*> handlers);
  CXXTryStmt(EmptyShell Empty, unsigned numHandlers)
    : Stmt(CXXTryStmtClass), NumHandlers(numHandlers) { }

  Stmt *const *getStmts() const { return getTrailingObjects<Stmt *>(); }
  Stmt **getStmts() { return getTrailingObjects<Stmt *>(); }

public:
  static CXXTryStmt *Create(const ASTContext &C, SourceLocation tryLoc,
                            Stmt *tryBlock, ArrayRef<Stmt*> handlers);

  static CXXTryStmt *Create(const ASTContext &C, EmptyShell Empty,
                            unsigned numHandlers);

  LLVM_ATTRIBUTE_DEPRECATED(SourceLocation getLocStart() const LLVM_READONLY,
                            "Use getBeginLoc instead") {
    return getBeginLoc();
  }
  SourceLocation getBeginLoc() const LLVM_READONLY { return getTryLoc(); }
  LLVM_ATTRIBUTE_DEPRECATED(SourceLocation getLocEnd() const LLVM_READONLY,
                            "Use getEndLoc instead") {
    return getEndLoc();
  }

  SourceLocation getTryLoc() const { return TryLoc; }
  SourceLocation getEndLoc() const {
    return getStmts()[NumHandlers]->getEndLoc();
  }

  CompoundStmt *getTryBlock() {
    return cast<CompoundStmt>(getStmts()[0]);
  }
  const CompoundStmt *getTryBlock() const {
    return cast<CompoundStmt>(getStmts()[0]);
  }

  unsigned getNumHandlers() const { return NumHandlers; }
  CXXCatchStmt *getHandler(unsigned i) {
    return cast<CXXCatchStmt>(getStmts()[i + 1]);
  }
  const CXXCatchStmt *getHandler(unsigned i) const {
    return cast<CXXCatchStmt>(getStmts()[i + 1]);
  }

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == CXXTryStmtClass;
  }

  child_range children() {
    return child_range(getStmts(), getStmts() + getNumHandlers() + 1);
  }
};

/// CXXForRangeStmt - This represents C++0x [stmt.ranged]'s ranged for
/// statement, represented as 'for (range-declarator : range-expression)'.
///
/// This is stored in a partially-desugared form to allow full semantic
/// analysis of the constituent components. The original syntactic components
/// can be extracted using getLoopVariable and getRangeInit.
class CXXForRangeStmt : public Stmt {
  SourceLocation ForLoc;
  enum { RANGE, BEGINSTMT, ENDSTMT, COND, INC, LOOPVAR, BODY, END };
  // SubExprs[RANGE] is an expression or declstmt.
  // SubExprs[COND] and SubExprs[INC] are expressions.
  Stmt *SubExprs[END];
  SourceLocation CoawaitLoc;
  SourceLocation ColonLoc;
  SourceLocation RParenLoc;

  friend class ASTStmtReader;
public:
  CXXForRangeStmt(DeclStmt *Range, DeclStmt *Begin, DeclStmt *End,
                  Expr *Cond, Expr *Inc, DeclStmt *LoopVar, Stmt *Body,
                  SourceLocation FL, SourceLocation CAL, SourceLocation CL,
                  SourceLocation RPL);
  CXXForRangeStmt(EmptyShell Empty) : Stmt(CXXForRangeStmtClass, Empty) { }


  VarDecl *getLoopVariable();
  Expr *getRangeInit();

  const VarDecl *getLoopVariable() const;
  const Expr *getRangeInit() const;


  DeclStmt *getRangeStmt() { return cast<DeclStmt>(SubExprs[RANGE]); }
  DeclStmt *getBeginStmt() {
    return cast_or_null<DeclStmt>(SubExprs[BEGINSTMT]);
  }
  DeclStmt *getEndStmt() { return cast_or_null<DeclStmt>(SubExprs[ENDSTMT]); }
  Expr *getCond() { return cast_or_null<Expr>(SubExprs[COND]); }
  Expr *getInc() { return cast_or_null<Expr>(SubExprs[INC]); }
  DeclStmt *getLoopVarStmt() { return cast<DeclStmt>(SubExprs[LOOPVAR]); }
  Stmt *getBody() { return SubExprs[BODY]; }

  const DeclStmt *getRangeStmt() const {
    return cast<DeclStmt>(SubExprs[RANGE]);
  }
  const DeclStmt *getBeginStmt() const {
    return cast_or_null<DeclStmt>(SubExprs[BEGINSTMT]);
  }
  const DeclStmt *getEndStmt() const {
    return cast_or_null<DeclStmt>(SubExprs[ENDSTMT]);
  }
  const Expr *getCond() const {
    return cast_or_null<Expr>(SubExprs[COND]);
  }
  const Expr *getInc() const {
    return cast_or_null<Expr>(SubExprs[INC]);
  }
  const DeclStmt *getLoopVarStmt() const {
    return cast<DeclStmt>(SubExprs[LOOPVAR]);
  }
  const Stmt *getBody() const { return SubExprs[BODY]; }

  void setRangeInit(Expr *E) { SubExprs[RANGE] = reinterpret_cast<Stmt*>(E); }
  void setRangeStmt(Stmt *S) { SubExprs[RANGE] = S; }
  void setBeginStmt(Stmt *S) { SubExprs[BEGINSTMT] = S; }
  void setEndStmt(Stmt *S) { SubExprs[ENDSTMT] = S; }
  void setCond(Expr *E) { SubExprs[COND] = reinterpret_cast<Stmt*>(E); }
  void setInc(Expr *E) { SubExprs[INC] = reinterpret_cast<Stmt*>(E); }
  void setLoopVarStmt(Stmt *S) { SubExprs[LOOPVAR] = S; }
  void setBody(Stmt *S) { SubExprs[BODY] = S; }

  SourceLocation getForLoc() const { return ForLoc; }
  SourceLocation getCoawaitLoc() const { return CoawaitLoc; }
  SourceLocation getColonLoc() const { return ColonLoc; }
  SourceLocation getRParenLoc() const { return RParenLoc; }

  LLVM_ATTRIBUTE_DEPRECATED(SourceLocation getLocStart() const LLVM_READONLY,
                            "Use getBeginLoc instead") {
    return getBeginLoc();
  }
  SourceLocation getBeginLoc() const LLVM_READONLY { return ForLoc; }
  LLVM_ATTRIBUTE_DEPRECATED(SourceLocation getLocEnd() const LLVM_READONLY,
                            "Use getEndLoc instead") {
    return getEndLoc();
  }
  SourceLocation getEndLoc() const LLVM_READONLY {
    return SubExprs[BODY]->getEndLoc();
  }

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == CXXForRangeStmtClass;
  }

  // Iterators
  child_range children() {
    return child_range(&SubExprs[0], &SubExprs[END]);
  }
};

/// \brief The base class of tuple and pack expansion statements.
///
/// Tuple and pack expansion statements have the following form:
///
/// \verbatim
///   for (auto x : expandable) statement
/// \endverbatim
///
/// The "expandable" expression is either a tuple-like construct or an
/// unexpanded parameter pack.
class CXXExpansionStmt : public Stmt {
protected:
  /// \brief The subexpressions of an expression include the loop variable,
  /// the tuple or pack being expanded, and the loop body.
  enum {
    RANGE, ///< The expression or captured variable being expanded.
    LOOP,  ///< The variable bound to each member of the expansion.
    BODY,  ///< The uninstantiated loop body.
    END
  };
  Stmt *SubExprs[END];

  SourceLocation ForLoc;
  SourceLocation EllipsisLoc;
  SourceLocation ColonLoc;
  SourceLocation RParenLoc;

  /// \brief The expansion size of the range. When the range is dependent
  /// this value is not meaningful.
  std::size_t Size;

  /// \brief The statements instantiated from the loop body. These are not
  /// sub-expressions.
  Stmt **InstantiatedStmts;

  friend class ASTStmtReader;

public:
  CXXExpansionStmt(StmtClass SC, DeclStmt *Range, DeclStmt *LoopVar, Stmt *Body,
                   std::size_t N, SourceLocation FL, SourceLocation EL,
                   SourceLocation CL, SourceLocation RPL);
  CXXExpansionStmt(StmtClass SC, EmptyShell Empty) : Stmt(SC, Empty) {}

  /// \brief Returns the statement containing the range declaration.
  Stmt *getRangeVarStmt() const { return SubExprs[RANGE]; }

  /// \brief The range variable.
  const VarDecl *getRangeVariable() const;  
  VarDecl *getRangeVariable();

  /// \brief Returns the dependent loop variable declaration.
  DeclStmt *getLoopVarStmt() const { return cast<DeclStmt>(SubExprs[LOOP]); }

  /// \brief Get the loop variable.
  const VarDecl *getLoopVariable() const;
  VarDecl *getLoopVariable();

  /// \brief Returns the parsed body of the loop.
  const Stmt *getBody() const { return SubExprs[BODY]; }
  Stmt *getBody() { return SubExprs[BODY]; }

  /// \brief Set the body of the loop.
  void setBody(Stmt *S) { SubExprs[BODY] = S; }

  /// \brief Returns the number of instantiated statements.
  std::size_t getSize() const { return Size; }

  /// \brief Set the sequence of instantiated statements.
  void setInstantiatedStatements(Stmt **S) {
    assert(!InstantiatedStmts && "instantiated statements already defined");
    InstantiatedStmts = S;
  }

  /// \brief Returns the sequence of instantiated statements.
  ArrayRef<Stmt *> getInstantiatedStatements() const {
    return llvm::makeArrayRef(InstantiatedStmts, Size);
  }

  /// \brief Returns a pointer to the first instantiated statement.
  Stmt **begin_instantiated_statements() const { return InstantiatedStmts; }
  /// \brief Returns a pointer past the last instantiated statement.
  Stmt **end_instantiated_statements() const {
    return InstantiatedStmts + Size;
  }

  SourceLocation getForLoc() const { return ForLoc; }
  SourceLocation getEllipsisLoc() const { return EllipsisLoc; }
  SourceLocation getColonLoc() const { return ColonLoc; }
  SourceLocation getRParenLoc() const { return RParenLoc; }

  LLVM_ATTRIBUTE_DEPRECATED(SourceLocation getLocStart() const LLVM_READONLY,
                            "Use getBeginLoc instead") {
    return getBeginLoc();
  }
  LLVM_ATTRIBUTE_DEPRECATED(SourceLocation getLocEnd() const LLVM_READONLY,
                            "Use getEndLoc instead") {
    return getEndLoc();
  }  

  SourceLocation getBeginLoc() const LLVM_READONLY { return ForLoc; }
  SourceLocation getEndLoc() const LLVM_READONLY {
    return SubExprs[BODY]->getEndLoc();
  }

  // Iterators
  child_range children() { return child_range(&SubExprs[0], &SubExprs[END]); }
};

/// Represents the expansion of a loop body for each element of a tuple.
///
/// For example:
///
/// \code
///   for (auto x : make_tuple(0, 1.2, "3"_s)) cout << x;
/// \endcode
///
/// Internally, the statement is represented in a partially desugared form like
/// this:
///
/// \verbatim
///   {
///     auto&& __tuple = range-init;
///     for<int __N> {
///       for-range-declaration = get<__N>(__tuple);
///       statement
///   }
/// \endverbatim
///
/// When the loop is finished (i.e., after parsing or instantiation), the
/// inner block of the \c for\<\> is expanded for each value \c __N from 0 to
/// the number of elements determined from the type of \c __tuple.
class CXXTupleExpansionStmt : public CXXExpansionStmt {
  /// Abstractly, the size of the expansion. This is needed to construct
  /// the initializer for the loop variable.
  TemplateParameterList *Parms;

public:
  CXXTupleExpansionStmt(TemplateParameterList *TP, DeclStmt *RangeVar,
                        DeclStmt *LoopVar, Stmt *Body, std::size_t N,
                        SourceLocation FL, SourceLocation EL, SourceLocation CL,
                        SourceLocation RPL);
  CXXTupleExpansionStmt(EmptyShell Empty)
      : CXXExpansionStmt(CXXTupleExpansionStmtClass, Empty) {}

  /// Returns the template parameter list of the declaration.
  TemplateParameterList *getTemplateParameters() { return Parms; }

  /// Returns the placeholder value \c __N used in the loop variable
  /// initializer \c get\<__N\>(__range).
  NonTypeTemplateParmDecl *getPlaceholderParameter();

  /// Returns the statement containing the range declaration.
  DeclStmt *getRangeVarStmt() const { return cast<DeclStmt>(SubExprs[RANGE]); }

  /// Returns the initializer of the range statement.
  Expr *getRangeInit();
  const Expr *getRangeInit() const;

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == CXXTupleExpansionStmtClass;
  }
};

/// Represents the expansion of a loop body for each element of
/// a constexpr range.
///
/// A constexpr range is range whose begin and end operations are
/// constexpr and whose iterator type is a literal type with all
/// salient iterator operations being constexpr.
/// For example:
///
/// \code
///   for constexpr (auto x : constexpr_range) cout << x;
/// \endcode
///
/// Internally, the statement is represented in a partially desugared form like
/// this:
///
/// \verbatim
///   {
///     auto&& __range = range-init;
///     for<iter> {
///       constexpr for-range-declaration = *__iter; 
///       statement
///       std::next(__iter);
///   }
/// \endverbatim
class CXXConstexprExpansionStmt : public CXXExpansionStmt {

  /// FIXME: Do constexpr expansions handle __N the same way as tupleexpansions?
  /// Abstractly, the size of the expansion. This is needed to construct
  /// the initializer for the loop variable.
  // TemplateParameterList *Parms;

  Stmt *BeginStmt = nullptr;
  Stmt *EndStmt = nullptr;
  Expr *BeginExpr = nullptr;
  Expr *NextCall = nullptr;  
  
public:
  CXXConstexprExpansionStmt(DeclStmt *RangeVar,
			    DeclStmt *LoopVar, Stmt *Body, std::size_t Size,
			    Stmt *BeginStmt, Stmt *EndStmt, Expr *BeginExpr,
			    Expr *NextCall, SourceLocation FL,
			    SourceLocation CEL, SourceLocation CL,
			    SourceLocation RPL);
  CXXConstexprExpansionStmt(EmptyShell Empty)
      : CXXExpansionStmt(CXXConstexprExpansionStmtClass, Empty) {}

  /// Returns the template parameter list of the declaration.
  TemplateParameterList *getTemplateParameters() { return nullptr; }

  /// Returns the placeholder value \c __N used in the loop variable
  /// initializer \c get\<__N\>(__range).
  NonTypeTemplateParmDecl *getPlaceholderParameter() { return nullptr; }

  /// Returns the statement containing the range declaration.
  DeclStmt *getRangeVarStmt() const { return cast<DeclStmt>(SubExprs[RANGE]); }

  Expr *getBeginExpr() const { return BeginExpr; }

  Expr *getNextCall() const { return NextCall; }

  /// Returns the initializer of the range statement.
  Expr *getRangeInit() { return nullptr; }
  const Expr *getRangeInit() const { return nullptr; }

  VarDecl *getBeginVariable();
  VarDecl *getEndVariable();

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == CXXConstexprExpansionStmtClass;
  }
};

/// \brief Represents the expansion of a parameter pack over a loop body.
class CXXPackExpansionStmt : public CXXExpansionStmt {
public:
  CXXPackExpansionStmt(DeclStmt *RangeVar, DeclStmt *LoopVar, Stmt *Body,
                       SourceLocation FL, SourceLocation EL, SourceLocation CL,
                       SourceLocation RPL);
  CXXPackExpansionStmt(EmptyShell Empty)
      : CXXExpansionStmt(CXXPackExpansionStmtClass, Empty) {}

  /// \brief Returns the unexpanded parameter pack being ranged over.
  Expr *getUnexpandedPack() { return cast<Expr>(SubExprs[RANGE]); }
  const Expr *getUnexpandedPack() const;

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == CXXPackExpansionStmtClass;
  }
};

/// Representation of a Microsoft __if_exists or __if_not_exists
/// statement with a dependent name.
///
/// The __if_exists statement can be used to include a sequence of statements
/// in the program only when a particular dependent name does not exist. For
/// example:
///
/// \code
/// template<typename T>
/// void call_foo(T &t) {
///   __if_exists (T::foo) {
///     t.foo(); // okay: only called when T::foo exists.
///   }
/// }
/// \endcode
///
/// Similarly, the __if_not_exists statement can be used to include the
/// statements when a particular name does not exist.
///
/// Note that this statement only captures __if_exists and __if_not_exists
/// statements whose name is dependent. All non-dependent cases are handled
/// directly in the parser, so that they don't introduce a new scope. Clang
/// introduces scopes in the dependent case to keep names inside the compound
/// statement from leaking out into the surround statements, which would
/// compromise the template instantiation model. This behavior differs from
/// Visual C++ (which never introduces a scope), but is a fairly reasonable
/// approximation of the VC++ behavior.
class MSDependentExistsStmt : public Stmt {
  SourceLocation KeywordLoc;
  bool IsIfExists;
  NestedNameSpecifierLoc QualifierLoc;
  DeclarationNameInfo NameInfo;
  Stmt *SubStmt;

  friend class ASTReader;
  friend class ASTStmtReader;

public:
  MSDependentExistsStmt(SourceLocation KeywordLoc, bool IsIfExists,
                        NestedNameSpecifierLoc QualifierLoc,
                        DeclarationNameInfo NameInfo,
                        CompoundStmt *SubStmt)
  : Stmt(MSDependentExistsStmtClass),
    KeywordLoc(KeywordLoc), IsIfExists(IsIfExists),
    QualifierLoc(QualifierLoc), NameInfo(NameInfo),
    SubStmt(reinterpret_cast<Stmt *>(SubStmt)) { }

  /// Retrieve the location of the __if_exists or __if_not_exists
  /// keyword.
  SourceLocation getKeywordLoc() const { return KeywordLoc; }

  /// Determine whether this is an __if_exists statement.
  bool isIfExists() const { return IsIfExists; }

  /// Determine whether this is an __if_exists statement.
  bool isIfNotExists() const { return !IsIfExists; }

  /// Retrieve the nested-name-specifier that qualifies this name, if
  /// any.
  NestedNameSpecifierLoc getQualifierLoc() const { return QualifierLoc; }

  /// Retrieve the name of the entity we're testing for, along with
  /// location information
  DeclarationNameInfo getNameInfo() const { return NameInfo; }

  /// Retrieve the compound statement that will be included in the
  /// program only if the existence of the symbol matches the initial keyword.
  CompoundStmt *getSubStmt() const {
    return reinterpret_cast<CompoundStmt *>(SubStmt);
  }

  LLVM_ATTRIBUTE_DEPRECATED(SourceLocation getLocStart() const LLVM_READONLY,
                            "Use getBeginLoc instead") {
    return getBeginLoc();
  }
  SourceLocation getBeginLoc() const LLVM_READONLY { return KeywordLoc; }
  LLVM_ATTRIBUTE_DEPRECATED(SourceLocation getLocEnd() const LLVM_READONLY,
                            "Use getEndLoc instead") {
    return getEndLoc();
  }
  SourceLocation getEndLoc() const LLVM_READONLY {
    return SubStmt->getEndLoc();
  }

  child_range children() {
    return child_range(&SubStmt, &SubStmt+1);
  }

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == MSDependentExistsStmtClass;
  }
};

/// Represents the body of a coroutine. This wraps the normal function
/// body and holds the additional semantic context required to set up and tear
/// down the coroutine frame.
class CoroutineBodyStmt final
    : public Stmt,
      private llvm::TrailingObjects<CoroutineBodyStmt, Stmt *> {
  enum SubStmt {
    Body,          ///< The body of the coroutine.
    Promise,       ///< The promise statement.
    InitSuspend,   ///< The initial suspend statement, run before the body.
    FinalSuspend,  ///< The final suspend statement, run after the body.
    OnException,   ///< Handler for exceptions thrown in the body.
    OnFallthrough, ///< Handler for control flow falling off the body.
    Allocate,      ///< Coroutine frame memory allocation.
    Deallocate,    ///< Coroutine frame memory deallocation.
    ReturnValue,   ///< Return value for thunk function: p.get_return_object().
    ResultDecl,    ///< Declaration holding the result of get_return_object.
    ReturnStmt,    ///< Return statement for the thunk function.
    ReturnStmtOnAllocFailure, ///< Return statement if allocation failed.
    FirstParamMove ///< First offset for move construction of parameter copies.
  };
  unsigned NumParams;

  friend class ASTStmtReader;
  friend class ASTReader;
  friend TrailingObjects;

  Stmt **getStoredStmts() { return getTrailingObjects<Stmt *>(); }

  Stmt *const *getStoredStmts() const { return getTrailingObjects<Stmt *>(); }

public:

  struct CtorArgs {
    Stmt *Body = nullptr;
    Stmt *Promise = nullptr;
    Expr *InitialSuspend = nullptr;
    Expr *FinalSuspend = nullptr;
    Stmt *OnException = nullptr;
    Stmt *OnFallthrough = nullptr;
    Expr *Allocate = nullptr;
    Expr *Deallocate = nullptr;
    Expr *ReturnValue = nullptr;
    Stmt *ResultDecl = nullptr;
    Stmt *ReturnStmt = nullptr;
    Stmt *ReturnStmtOnAllocFailure = nullptr;
    ArrayRef<Stmt *> ParamMoves;
  };

private:

  CoroutineBodyStmt(CtorArgs const& Args);

public:
  static CoroutineBodyStmt *Create(const ASTContext &C, CtorArgs const &Args);
  static CoroutineBodyStmt *Create(const ASTContext &C, EmptyShell,
                                   unsigned NumParams);

  bool hasDependentPromiseType() const {
    return getPromiseDecl()->getType()->isDependentType();
  }

  /// Retrieve the body of the coroutine as written. This will be either
  /// a CompoundStmt or a TryStmt.
  Stmt *getBody() const {
    return getStoredStmts()[SubStmt::Body];
  }

  Stmt *getPromiseDeclStmt() const {
    return getStoredStmts()[SubStmt::Promise];
  }
  VarDecl *getPromiseDecl() const {
    return cast<VarDecl>(cast<DeclStmt>(getPromiseDeclStmt())->getSingleDecl());
  }

  Stmt *getInitSuspendStmt() const {
    return getStoredStmts()[SubStmt::InitSuspend];
  }
  Stmt *getFinalSuspendStmt() const {
    return getStoredStmts()[SubStmt::FinalSuspend];
  }

  Stmt *getExceptionHandler() const {
    return getStoredStmts()[SubStmt::OnException];
  }
  Stmt *getFallthroughHandler() const {
    return getStoredStmts()[SubStmt::OnFallthrough];
  }

  Expr *getAllocate() const {
    return cast_or_null<Expr>(getStoredStmts()[SubStmt::Allocate]);
  }
  Expr *getDeallocate() const {
    return cast_or_null<Expr>(getStoredStmts()[SubStmt::Deallocate]);
  }
  Expr *getReturnValueInit() const {
    return cast<Expr>(getStoredStmts()[SubStmt::ReturnValue]);
  }
  Stmt *getResultDecl() const { return getStoredStmts()[SubStmt::ResultDecl]; }
  Stmt *getReturnStmt() const { return getStoredStmts()[SubStmt::ReturnStmt]; }
  Stmt *getReturnStmtOnAllocFailure() const {
    return getStoredStmts()[SubStmt::ReturnStmtOnAllocFailure];
  }
  ArrayRef<Stmt const *> getParamMoves() const {
    return {getStoredStmts() + SubStmt::FirstParamMove, NumParams};
  }

  LLVM_ATTRIBUTE_DEPRECATED(SourceLocation getLocStart() const LLVM_READONLY,
                            "Use getBeginLoc instead") {
    return getBeginLoc();
  }
  SourceLocation getBeginLoc() const LLVM_READONLY {
    return getBody() ? getBody()->getBeginLoc()
                     : getPromiseDecl()->getBeginLoc();
  }
  LLVM_ATTRIBUTE_DEPRECATED(SourceLocation getLocEnd() const LLVM_READONLY,
                            "Use getEndLoc instead") {
    return getEndLoc();
  }
  SourceLocation getEndLoc() const LLVM_READONLY {
    return getBody() ? getBody()->getEndLoc() : getPromiseDecl()->getEndLoc();
  }

  child_range children() {
    return child_range(getStoredStmts(),
                       getStoredStmts() + SubStmt::FirstParamMove + NumParams);
  }

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == CoroutineBodyStmtClass;
  }
};

/// Represents a 'co_return' statement in the C++ Coroutines TS.
///
/// This statament models the initialization of the coroutine promise
/// (encapsulating the eventual notional return value) from an expression
/// (or braced-init-list), followed by termination of the coroutine.
///
/// This initialization is modeled by the evaluation of the operand
/// followed by a call to one of:
///   <promise>.return_value(<operand>)
///   <promise>.return_void()
/// which we name the "promise call".
class CoreturnStmt : public Stmt {
  SourceLocation CoreturnLoc;

  enum SubStmt { Operand, PromiseCall, Count };
  Stmt *SubStmts[SubStmt::Count];

  bool IsImplicit : 1;

  friend class ASTStmtReader;
public:
  CoreturnStmt(SourceLocation CoreturnLoc, Stmt *Operand, Stmt *PromiseCall,
               bool IsImplicit = false)
      : Stmt(CoreturnStmtClass), CoreturnLoc(CoreturnLoc),
        IsImplicit(IsImplicit) {
    SubStmts[SubStmt::Operand] = Operand;
    SubStmts[SubStmt::PromiseCall] = PromiseCall;
  }

  CoreturnStmt(EmptyShell) : CoreturnStmt({}, {}, {}) {}

  SourceLocation getKeywordLoc() const { return CoreturnLoc; }

  /// Retrieve the operand of the 'co_return' statement. Will be nullptr
  /// if none was specified.
  Expr *getOperand() const { return static_cast<Expr*>(SubStmts[Operand]); }

  /// Retrieve the promise call that results from this 'co_return'
  /// statement. Will be nullptr if either the coroutine has not yet been
  /// finalized or the coroutine has no eventual return type.
  Expr *getPromiseCall() const {
    return static_cast<Expr*>(SubStmts[PromiseCall]);
  }

  bool isImplicit() const { return IsImplicit; }
  void setIsImplicit(bool value = true) { IsImplicit = value; }

  LLVM_ATTRIBUTE_DEPRECATED(SourceLocation getLocStart() const LLVM_READONLY,
                            "Use getBeginLoc instead") {
    return getBeginLoc();
  }
  SourceLocation getBeginLoc() const LLVM_READONLY { return CoreturnLoc; }
  LLVM_ATTRIBUTE_DEPRECATED(SourceLocation getLocEnd() const LLVM_READONLY,
                            "Use getEndLoc instead") {
    return getEndLoc();
  }
  SourceLocation getEndLoc() const LLVM_READONLY {
    return getOperand() ? getOperand()->getEndLoc() : getBeginLoc();
  }

  child_range children() {
    if (!getOperand())
      return child_range(SubStmts + SubStmt::PromiseCall,
                         SubStmts + SubStmt::Count);
    return child_range(SubStmts, SubStmts + SubStmt::Count);
  }

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == CoreturnStmtClass;
  }
};

}  // end namespace clang

#endif
