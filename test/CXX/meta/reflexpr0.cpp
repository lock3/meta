// RUN: %clang_cc1 -std=c++1z -freflection %s

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
struct class_tmpl {
  T function() { return T(); }
  constexpr T constexpr_function() { return T(); }

  T variable;
  static T static_variable = T();
  static constexpr T constexpr_variable = T();
};

namespace N {
  namespace M {
    enum E { A, B, C };
    enum class EC { A, B, C };
  }
}

constexpr int test() {
  auto r1 = reflexpr(S);

  auto r2 = reflexpr(E);

  auto r3 = reflexpr(A);

  auto r4 = reflexpr(EC);

  auto r5 = reflexpr(EC::A);

  auto r6 = reflexpr(f);

  auto r7 = reflexpr(global);

  auto r8 = reflexpr(ovl);

  auto r9 = reflexpr(fn_tmpl);

  // auto r10 = reflexpr(class_tmpl);

  auto r11 = reflexpr(class_tmpl<int>);

  // auto r12 = reflexpr(class_tmpl<int>::function);

  // auto r13 = reflexpr(class_tmpl<int>::constexpr_function);

  auto r14 = reflexpr(class_tmpl<int>::variable);

  auto r15 = reflexpr(class_tmpl<int>::constexpr_variable);

  auto r16 = reflexpr(class_tmpl<int>::static_variable);

  auto r17 = reflexpr(N);

  auto r18 = reflexpr(N::M);

  auto r19 = reflexpr(S::E);

  auto r20 = reflexpr(S::X);

  auto r21 = reflexpr(S::EC);

  auto r22 = reflexpr(S::EC::X);

  // auto r23 = reflexpr(S::function);

  // auto r24 = reflexpr(S::constexpr_function);

  auto r25 = reflexpr(S::variable);

  auto r26 = reflexpr(S::constexpr_variable);

  auto r27 = reflexpr(class_tmpl<int>::static_variable);

  auto r28 = reflexpr(N::M::E);

  auto r29 = reflexpr(N::M::A);

  auto r30 = reflexpr(N::M::EC);

  auto r31 = reflexpr(N::M::EC::A);

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
  // auto r = reflexpr(X);
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
