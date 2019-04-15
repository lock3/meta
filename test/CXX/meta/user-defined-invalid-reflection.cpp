// RUN: %clang_cc1 -freflection -verify -std=c++1z %s

namespace meta {
  using info = decltype(reflexpr(void));
}

constexpr meta::info invalid_refl = __invalid_reflection("custom error message");

int idexpr_test() {
  return idexpr(invalid_refl); // expected-error {{cannot reify invalid reflection}} expected-note {{custom error message}}
}

int unqualid_test() {
  return unqualid(invalid_refl); // expected-error {{cannot reify invalid reflection}} expected-note {{custom error message}}
}

using ReflectedType = typename(invalid_refl); // expected-error {{cannot reify invalid reflection}} expected-note {{custom error message}}

template<typename T>
constexpr int foo() {
  return T();
}

constexpr int fcall_result = foo<templarg(invalid_refl)>(); // expected-error {{cannot reify invalid reflection}} expected-note {{custom error message}}

constexpr int valueof_result = valueof(invalid_refl); // expected-error {{cannot reify invalid reflection}} expected-note {{custom error message}}
