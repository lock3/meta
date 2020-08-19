// RUN: %clang_cc1 -freflection -Wno-deprecated-fragment -std=c++2a -fsyntax-only -verify %s

namespace meta {
  using info = decltype(reflexpr(void));
}

template <typename... Args>
void dummy(Args...) {}

consteval void impl(meta::info source) {
  int a = 1;
  -> __fragment struct S {
    void foo() {
      dummy(unqualid("foo_", a)); // expected-error {{use of undeclared identifier 'foo_1'}}
    }
  };
}

class(impl) foo {
};
