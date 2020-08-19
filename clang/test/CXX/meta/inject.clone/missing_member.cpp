// RUN: %clang_cc1 -std=c++2a -freflection -verify %s

struct foo {
  int b; // expected-note {{'b' declared here}}

  int bar() {
    return b;
  }
};

class foo_derived {
  consteval -> reflexpr(foo::bar);
  // expected-error@-1 {{a clone of 'b' was required, but was not present in the injectee 'foo_derived'}}
};
