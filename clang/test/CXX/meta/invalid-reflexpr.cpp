// RUN: %clang_cc1 -freflection -verify -std=c++1z %s

namespace meta {
  using info = decltype(reflexpr(void));
}

constexpr meta::info invalid_refl = reflexpr(); // expected-error {{expected expression}} expected-note+ {{declared here}}

int expr_splice_test() {
  return [<invalid_refl>]; // expected-error {{reflection is not a constant expression}} expected-note {{initializer of 'invalid_refl' is unknown}}
}

int identifier_splice_test() {
  return [# invalid_refl #]; // expected-error {{reflection is not a constant expression}} expected-note {{initializer of 'invalid_refl' is unknown}}
}

using ReflectedType = typename [<invalid_refl>]; // expected-error {{reflection is not a constant expression}} expected-note {{initializer of 'invalid_refl' is unknown}}

template<typename T>
constexpr int foo() {
  return T();
}

constexpr meta::info invalid_refl_arr [] = { reflexpr() }; // expected-error {{expected expression}}
constexpr int fcall_result = foo<...[< invalid_refl_arr >]...>(); // expected-error {{cannot expand expression}}
