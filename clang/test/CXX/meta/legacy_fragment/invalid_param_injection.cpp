// RUN: %clang_cc1 -freflection -Wno-deprecated-fragment -fsyntax-only -verify -std=c++2a %s

#include "../reflection_iterator.h"

struct bar { // expected-note {{declared here}}
  void do_thing(int a, int b) {
  }

  void undefined_do_thing(int a, int b);
};

constexpr int add(int a, int b) {
  return a + b;
}

constexpr meta::info invalid_params [] = { __invalid_reflection("bang!") };

class invalid_reflection {
  consteval {
    {
      -> __fragment struct {
        constexpr int new_do_thing(-> invalid_params) const { // expected-error {{no return statement in constexpr function}} expected-error {{cannot reify invalid reflection}} expected-note {{bang!}}
          return add(unqualid(... invalid_params)); // expected-error {{cannot reify invalid reflection}} expected-note {{bang!}}
        }
      };
    }
  }
};

constexpr meta::info wrong_type_params [] = { reflexpr(bar) };

class wrong_reflection_kind {
  consteval {
    {
      -> __fragment struct {
        constexpr int new_do_thing(-> wrong_type_params) const { // expected-error {{no return statement in constexpr function}} expected-error {{reflection does not reflect a parameter}}
          return add(unqualid(... wrong_type_params)); // expected-error {{'bar' does not refer to a value}}
        }
      };
    }
  }
};
