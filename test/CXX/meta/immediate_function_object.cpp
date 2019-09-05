// RUN: %clang_cc1 -std=c++2a -freflection -fsyntax-only -verify %s

namespace meta {
  using info = decltype(reflexpr(void));
}

consteval int bar(meta::info arg) { return 0; }

template<typename T>
struct consteval_foo {
  T callee;

  consteval consteval_foo(T callee) : callee(callee) { }

  template<typename R>
  consteval auto operator()(R arg) const {
    return callee(arg);
  }
};

template<typename T>
struct constexpr_foo { // expected-note {{candidate template ignored: could not match 'constexpr_foo<T>' against 'int (*)(meta::info)}}
  T callee;

  constexpr constexpr_foo(T callee) : callee(callee) { } // expected-note {{candidate function [with T = int (*)(meta::info)] not viable: no known conversion from 'int (meta::info)' to 'int (*)(meta::info)' for 1st argument; take the address of the argument with &}}

  template<typename R>
  constexpr auto operator()(R arg) const {
    return callee(arg);
  }
};

constexpr struct check_reflection_fn {
  consteval bool operator()(meta::info type) const {
    return true;
  }
} check_reflection;

int main() {
  int consteval_implicit_res = consteval_foo(bar)(reflexpr(int));
  int consteval_explicit_res = consteval_foo(&bar)(reflexpr(int));
  auto constexpr_implicit_res = constexpr_foo(bar); // expected-error {{no viable constructor or deduction guide for deduction of template arguments of 'constexpr_foo}}
  auto constexpr_explicit_res = constexpr_foo(&bar); // expected-error {{immediate functions can only be converted to function pointers inside of an immediate function context or, as part of an immediate invocation}}
  return 0;
}
