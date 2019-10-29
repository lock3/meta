// RUN: %clang_cc1 -std=c++2a -freflection -fsyntax-only -verify %s

consteval int foo() { return 0; }

template<typename F>
consteval auto consteval_test(F callee) {
  return callee();
}

template<typename F>
constexpr auto constexpr_test(F callee) {
  return callee();
}

int main() {
  int consteval_implicit_res = consteval_test(foo);
  int consteval_explicit_res = consteval_test(&foo);
  int constexpr_implicit_res = constexpr_test(foo); // expected-error {{immediate functions can only be converted to function pointers inside of an immediate function context or, as part of an immediate invocation}}
  int constexpr_explicit_res = constexpr_test(&foo); // expected-error {{immediate functions can only be converted to function pointers inside of an immediate function context or, as part of an immediate invocation}}
  return 0;
}
