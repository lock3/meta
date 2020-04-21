// RUN: %clang_cc1 -std=c++2a -freflection -verify %s

int foo() {
  -> fragment {}; // expected-error {{injection statements may only appear in manifestly constant evaluated contexts}}
}
