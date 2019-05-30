// RUN: %clang_cc1 -std=c++2a -freflection -verify %s

int foo() {
  -> __fragment {}; // expected-error {{injection statements can only appear in constexpr contexts}}
}
