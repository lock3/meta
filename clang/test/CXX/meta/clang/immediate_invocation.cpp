// RUN: %clang_cc1 -std=c++2a -freflection -fsyntax-only -verify %s

consteval int foo() { return 0; } // expected-note {{declared here}} expected-note {{declared here}}

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
  int constexpr_implicit_res = constexpr_test(foo); // expected-error {{cannot take address of consteval function 'foo' outside of an immediate invocation}}
  int constexpr_explicit_res = constexpr_test(&foo); // expected-error {{cannot take address of consteval function 'foo' outside of an immediate invocation}}
  return 0;
}
