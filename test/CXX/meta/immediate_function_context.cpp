// RUN: %clang_cc1 -std=c++2a -freflection -fsyntax-only -verify %s

consteval int foo() { return 0; }

template<typename F>
constexpr bool test(F callee) {
  return callee();
}

consteval bool consteval_implicit_test() {
  return test(foo);
}

consteval bool consteval_explicit_test() {
  return test(&foo);
}

constexpr bool constexpr_implicit_test() { // expected-error {{no return statement in constexpr function}}
  return test(foo); // expected-error {{immediate functions can only be converted to function pointers inside of an immediate function context or, as part of an immediate invocation}}
}

constexpr bool constexpr_explicit_test() {
  return test(&foo); // expected-error {{immediate functions can only be converted to function pointers inside of an immediate function context or, as part of an immediate invocation}}
}

int main() {
  int consteval_implicit_res = consteval_implicit_test();
  int consteval_explicit_res = consteval_explicit_test();
  int constexpr_implicit_res = constexpr_implicit_test();
  int constexpr_explicit_res = constexpr_explicit_test();
  return 0;
}
