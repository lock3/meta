// RUN: %clang_cc1 -std=c++1z -freflection %s

#include "reflection_query.h"

struct assertion { };

#define assert(E) if (!(E)) __builtin_abort();

struct S {
  enum E { X, Y };
  enum class EC { X, Y };

  void function() { }
  constexpr void constexpr_function() { }

  int variable;
  static const int static_variable = 0;
  static constexpr int constexpr_variable = 0;
};

enum E { A, B, C };
enum class EC { A, B, C };

void f() { }

int global;

void ovl();
void ovl(int);

template<typename T>
void fn_tmpl() { }

template<typename T>
struct ClassTemplate {
  T function() { return T(); }
  constexpr T constexpr_function() { return T(); }

  T variable;
  static const T static_variable = T();
  static constexpr T constexpr_variable = T();
};

namespace N {
  namespace M {
    enum E { A, B, C };
    enum class EC { A, B, C };
  }
}

constexpr int test() {
  // FIXME: 
  // auto r0 = reflexpr(::);
  // assert(__reflect(query_is_namespace(r0)));

  auto r1 = reflexpr(S);
  assert(__reflect(query_is_type, r1));
  assert(__reflect(query_is_class, r1));
  assert(!__reflect(query_is_namespace, r1));

  auto r2 = reflexpr(E);
  assert(__reflect(query_is_type, r2));
  assert(__reflect(query_is_enum, r2));

  auto r3 = reflexpr(A);
  assert(__reflect(query_is_expression, r3));
  assert(__reflect(query_is_enumerator, r3));

  auto r4 = reflexpr(EC);
  assert(__reflect(query_is_type, r4));
  assert(__reflect(query_is_enum, r4));
  assert(__reflect(query_is_scoped_enum, r4));

  auto r5 = reflexpr(EC::A);
  assert(__reflect(query_is_expression, r5));
  assert(__reflect(query_is_enumerator, r5));

  auto r6 = reflexpr(f);
  assert(__reflect(query_is_expression, r6));
  assert(__reflect(query_is_function, r6));

  auto r7 = reflexpr(global);

  auto r8 = reflexpr(ovl);

  auto r9 = reflexpr(fn_tmpl);

  auto r10 = reflexpr(ClassTemplate);

  auto r11 = reflexpr(ClassTemplate<int>);

  auto r12 = reflexpr(ClassTemplate<int>::function);

  auto r13 = reflexpr(ClassTemplate<int>::constexpr_function);

  auto r14 = reflexpr(ClassTemplate<int>::variable);

  auto r15 = reflexpr(ClassTemplate<int>::constexpr_variable);

  auto r16 = reflexpr(ClassTemplate<int>::static_variable);

  auto r17 = reflexpr(N);

  auto r18 = reflexpr(N::M);

  auto r19 = reflexpr(S::E);
  // assert(__reflect(query_get_parent, r13) == reflexpr(S));
  (void)__reflect(query_get_parent, r13);

  auto r20 = reflexpr(S::X);
  (void)__reflect(query_get_type, r14);

  auto r21 = reflexpr(S::EC);

  auto r22 = reflexpr(S::EC::X);

  auto r23 = reflexpr(S::function);

  auto r24 = reflexpr(S::constexpr_function);

  auto r25 = reflexpr(S::variable);

  auto r26 = reflexpr(S::constexpr_variable);

  auto r27 = reflexpr(ClassTemplate<int>::static_variable);

  auto r28 = reflexpr(S::E);

  auto r29 = reflexpr(S::X);

  auto r30 = reflexpr(S::EC);

  auto r31 = reflexpr(S::EC::X);

  auto r32 = reflexpr(N::M::E);

  auto r33 = reflexpr(N::M::A);

  auto r34 = reflexpr(N::M::EC);

  auto r35 = reflexpr(N::M::EC::A);

  return 0;
}

template<typename T>
constexpr int type_template() {
  auto r = reflexpr(T);
  return 0;
}

template<int N>
constexpr int nontype_template() {
  auto r = reflexpr(N);
  return 0;
}

template<template<typename> class X>
constexpr int template_template() {
  auto r = reflexpr(X);
  return 0;
}

template<typename T>
struct temp { };

int main() {
  constexpr int n = test();

  constexpr int t = type_template<int>();
  constexpr int v = nontype_template<0>();
  constexpr int x = template_template<temp>();
}
