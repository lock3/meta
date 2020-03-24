// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "reflection_query.h"

struct assertion { };

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
  constexpr auto r0 = reflexpr(::);
  static_assert(__reflect(query_is_namespace, r0));

  constexpr auto r1 = reflexpr(S);
  static_assert(__reflect(query_is_type, __reflect(query_get_type, r1)));
  static_assert(__reflect(query_is_class, r1));
  static_assert(!__reflect(query_is_namespace, r1));

  constexpr auto r2 = reflexpr(E);
  static_assert(__reflect(query_is_type, r2));
  static_assert(__reflect(query_is_unscoped_enum, r2));

  constexpr auto r3 = reflexpr(A);
  static_assert(__reflect(query_is_expression, r3));
  static_assert(__reflect(query_is_enumerator, r3));

  constexpr auto r4 = reflexpr(EC);
  static_assert(__reflect(query_is_type, r4));
  static_assert(__reflect(query_is_unscoped_enum_type, r4));
  static_assert(__reflect(query_is_scoped_enum_type, r4));

  constexpr auto r5 = reflexpr(EC::A);
  static_assert(__reflect(query_is_expression, r5));
  static_assert(__reflect(query_is_enumerator, r5));

  constexpr auto r6 = reflexpr(f);
  static_assert(__reflect(query_is_expression, r6));
  static_assert(__reflect(query_is_function, r6));

  constexpr auto r7 = reflexpr(global);

  constexpr auto r8 = reflexpr(ovl);

  constexpr auto r9 = reflexpr(fn_tmpl);

  constexpr auto r10 = reflexpr(ClassTemplate);

  constexpr auto r11 = reflexpr(ClassTemplate<int>);

  constexpr auto r12 = reflexpr(ClassTemplate<int>::function);

  constexpr auto r13 = reflexpr(ClassTemplate<int>::constexpr_function);

  constexpr auto r14 = reflexpr(ClassTemplate<int>::variable);

  constexpr auto r15 = reflexpr(ClassTemplate<int>::constexpr_variable);

  constexpr auto r16 = reflexpr(ClassTemplate<int>::static_variable);

  constexpr auto r17 = reflexpr(N);

  constexpr auto r18 = reflexpr(N::M);

  constexpr auto r19 = reflexpr(S::E);
  // FIXME: This is awkward.
  static_assert(__reflect(query_get_type, __reflect(query_get_parent, r19)) == reflexpr(S));

  constexpr auto r20 = reflexpr(S::X);
  (void)__reflect(query_get_type, r14);

  constexpr auto r21 = reflexpr(S::EC);

  constexpr auto r22 = reflexpr(S::EC::X);

  constexpr auto r23 = reflexpr(S::function);

  constexpr auto r24 = reflexpr(S::constexpr_function);

  constexpr auto r25 = reflexpr(S::variable);

  constexpr auto r26 = reflexpr(S::constexpr_variable);

  constexpr auto r27 = reflexpr(ClassTemplate<int>::static_variable);

  constexpr auto r28 = reflexpr(S::E);

  constexpr auto r29 = reflexpr(S::X);

  constexpr auto r30 = reflexpr(S::EC);

  constexpr auto r31 = reflexpr(S::EC::X);

  constexpr auto r32 = reflexpr(N::M::E);

  constexpr auto r33 = reflexpr(N::M::A);

  constexpr auto r34 = reflexpr(N::M::EC);

  constexpr auto r35 = reflexpr(N::M::EC::A);

  return 0;
}

template<typename T>
consteval int type_template() {
  auto r = reflexpr(T);
  return 0;
}

template<int N>
consteval int nontype_template() {
  auto r = reflexpr(N);
  return 0;
}

template<template<typename> class X>
consteval int template_template() {
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
