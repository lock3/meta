// RUN: %clang_cc1 -freflection -verify -std=c++2a %s

namespace meta {
  using info = decltype(^void);
}

namespace non_template_reflection {
  constexpr meta::info invalid_refl = __invalid_reflection("custom error message");

  int expr_splice_test() {
    return [:invalid_refl:];
    // expected-error@-1 {{cannot splice invalid reflection}}
    // expected-note@-2 {{custom error message}}
  }

  using ReflectedType = typename [:invalid_refl:];
  // expected-error@-1 {{cannot splice invalid reflection}}
  // expected-note@-2 {{custom error message}}

  template<typename T>
  constexpr int foo() {
    return T();
  }

  constexpr meta::info invalid_refl_arr [] = { invalid_refl };
  constexpr int fcall_result = foo<...[: invalid_refl_arr :]...>();
  // expected-error@-1 {{cannot splice invalid reflection}}
  // expected-note@-2 {{custom error message}}
  // expected-note@-3 {{while splicing a non-dependent pack declared here}}
}

namespace template_reflection {
  template<int V>
  constexpr meta::info invalid_refl = __invalid_reflection(__concatenate("Error code: ", V));

  int expr_splice_test() {
    return [:invalid_refl<1>:];
    // expected-error@-1 {{cannot splice invalid reflection}}
    // expected-note@-2 {{Error code: 1}}
  }

  using ReflectedType = typename [:invalid_refl<1>:];
  // expected-error@-1 {{cannot splice invalid reflection}}
  // expected-note@-2 {{Error code: 1}}

  template<typename T>
  constexpr int foo() {
    return T();
  }

  constexpr meta::info invalid_refl_arr [] = { invalid_refl<1> };
  constexpr int fcall_result = foo<...[: invalid_refl_arr :]...>();
  // expected-error@-1 {{cannot splice invalid reflection}}
  // expected-note@-2 {{Error code: 1}}
  // expected-note@-3 {{while splicing a non-dependent pack declared here}}
}
