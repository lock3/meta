// RUN: %clang_cc1 -fsyntax-only -freflection -Wno-deprecated-fragment -std=c++2a -verify %s

namespace mismatch_constexpr {
constexpr auto frag = __fragment {
  requires constexpr int f1(); // expected-error {{constexpr declaration of 'f1' follows non-constexpr declaration}}
  requires int f2(); // expected-error {{non-constexpr declaration of 'f2' follows constexpr declaration}}
};

int f1() { return 0; } // expected-note {{previous declaration is here}}
constexpr int f2() { return 0; } // expected-note {{previous declaration is here}}

void test() {
  consteval -> frag;
}
}; // namespace mismatch-constexpr

namespace ovl {
constexpr auto frag = __fragment {
  requires int f1(); // expected-error {{use of undeclared required declarator.}} // expected-error {{no matching function for call to 'f1'}}
  requires int f1(int a);
};
int f1(int a) { return a; } // expected-note {{candidate function not viable: requires single argument 'a', but no arguments were provided}}

void test() {
  consteval -> frag;
}
}; // namespace ovl

namespace ambiguous {
constexpr auto frag = __fragment {
  requires int f(); // expected-error {{call to 'f' is ambiguous}} // expected-error {{use of undeclared required declarator.}}
};

int f() { return 0; } // expected-note {{candidate function}}
int f(int a = 42) { return 42; } // expected-note {{candidate function}}

void test() {
  consteval -> frag;
}
}; // namespace ambiguous

namespace default_args {
constexpr auto frag = __fragment {
  requires int f();
  requires int f(int a);
  requires int f(int a, int b);

#define assert(E) if (!(E)) __builtin_abort();
  assert(f() == 0);
  assert(f(1) == 42);
  assert(f(1, 2) == 42);
};

int f() { return 0; }
int f(int a, int b = 42) { return b; }

  void test() { consteval -> frag; }
}; // namespace default_args

int main() {
  mismatch_constexpr::test();
  ovl::test();
  ambiguous::test();
  default_args::test();
}
