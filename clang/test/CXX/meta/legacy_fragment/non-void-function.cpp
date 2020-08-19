// RUN: %clang_cc1 -verify -fsyntax-only -freflection -Wno-deprecated-fragment -std=c++2a %s

consteval -> __fragment namespace {
  int reset_foo() {
  } // expected-warning {{non-void function does not return a value}}
};

struct Thing {
  consteval -> __fragment struct {
    int reset_foo() {
    } // expected-warning {{non-void function does not return a value}}
  };
};
