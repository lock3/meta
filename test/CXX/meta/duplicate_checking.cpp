// RUN: %clang_cc1 -verify -fsyntax-only -freflection -std=c++1z %s

struct Foo {
  int method_a() { } // expected-note {{previous definition is here}}

  consteval -> __fragment struct {
    int method_a() { } // expected-error {{class member cannot be redeclared}}
  };
};
