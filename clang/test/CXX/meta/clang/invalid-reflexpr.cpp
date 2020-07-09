// RUN: %clang_cc1 -freflection -verify -std=c++2a %s

namespace meta {
  using info = decltype(reflexpr(void));
}

constexpr meta::info invalid_refl = reflexpr(); // expected-error {{expected expression}}

int idexpr_test() {
  return idexpr(invalid_refl); // expected-error {{reflection is not a constant expression}} expected-note {{subexpression not valid in a constant expression}}
}

int unqualid_test() {
  return unqualid(invalid_refl); // expected-error {{reflection is not a constant expression}} expected-note {{subexpression not valid in a constant expression}} expected-error {{expected unqualified-id}}
}

using ReflectedType = typename(invalid_refl); // expected-error {{reflection is not a constant expression}} expected-note {{subexpression not valid in a constant expression}}

template<typename T>
constexpr int foo() {
  return T();
}

constexpr int fcall_result = foo<templarg(invalid_refl)>(); // expected-error {{reflection is not a constant expression}} expected-note {{subexpression not valid in a constant expression}}

constexpr int valueof_result = valueof(invalid_refl); // expected-error {{reflection is not a constant expression}} expected-note {{subexpression not valid in a constant expression}}
