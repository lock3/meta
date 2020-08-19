// RUN: %clang_cc1 -std=c++2a -freflection -fsyntax-only -verify %s

namespace meta {
  using info = decltype(reflexpr(void));
}

consteval int bar(meta::info arg) { return 0; } // expected-note {{declared here}} expected-note {{declared here}}

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
struct constexpr_foo {
  T callee;

  constexpr constexpr_foo(T callee) : callee(callee) { }

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
  auto constexpr_implicit_res = constexpr_foo(bar); // expected-error {{cannot take address of consteval function 'bar' outside of an immediate invocation}}
  auto constexpr_explicit_res = constexpr_foo(&bar); // expected-error {{cannot take address of consteval function 'bar' outside of an immediate invocation}}
  return 0;
}
