// RUN: %clang_cc1 -freflection -verify -std=c++2a %s

namespace foo_ns {
  struct foo {
    static int i;
  };
}

auto y = foo_ns::typename(reflexpr(foo_ns::foo))::i; // expected-error {{the typename reifier cannot be preceded by a nested name specifier}}