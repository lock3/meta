// RUN: %clang_cc1 -std=c++2a -freflection -Wno-deprecated-fragment -verify %s

int foo() {
  -> __fragment {}; // expected-error {{injection statements may only appear in manifestly constant evaluated contexts}}
}
