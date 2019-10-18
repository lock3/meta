// RUN: %clang_cc1 -verify -fsyntax-only -freflection -std=c++2a %s

consteval -> __fragment namespace {
  int reset_foo() {
  } // expected-warning {{control reaches end of non-void function}}
};

struct Thing {
  consteval -> __fragment struct {
    int reset_foo() {
    } // expected-warning {{control reaches end of non-void function}}
  };
};
