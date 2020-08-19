// RUN: %clang_cc1 -freflection -verify -std=c++1z %s

namespace meta {
  using info = decltype(reflexpr(void));
}

constexpr meta::info invalid_refl = reflexpr(); // expected-error {{expected expression}} expected-note {{declared here}} expected-note {{declared here}} expected-note {{declared here}} expected-note {{declared here}} expected-note {{declared here}}

int idexpr_test() {
  return idexpr(invalid_refl); // expected-error {{reflection is not a constant expression}} expected-note {{initializer of 'invalid_refl' is unknown}}
}

int unqualid_test() {
  return unqualid(invalid_refl); // expected-error {{reflection is not a constant expression}} expected-note {{initializer of 'invalid_refl' is unknown}}
}

using ReflectedType = typename(invalid_refl); // expected-error {{reflection is not a constant expression}} expected-note {{initializer of 'invalid_refl' is unknown}}

template<typename T>
constexpr int foo() {
  return T();
}

constexpr int fcall_result = foo<templarg(invalid_refl)>(); // expected-error {{reflection is not a constant expression}} expected-note {{initializer of 'invalid_refl' is unknown}}

constexpr int valueof_result = valueof(invalid_refl); // expected-error {{reflection is not a constant expression}} expected-note {{initializer of 'invalid_refl' is unknown}}
