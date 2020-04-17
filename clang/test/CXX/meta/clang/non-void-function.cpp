// RUN: %clang_cc1 -verify -fsyntax-only -freflection -std=c++2a %s

consteval -> fragment namespace {
  int reset_foo() {
  } // expected-warning {{non-void function does not return a value}}
};

struct Thing {
  consteval -> fragment struct {
    int reset_foo() {
    } // expected-warning {{non-void function does not return a value}}
  };
};
