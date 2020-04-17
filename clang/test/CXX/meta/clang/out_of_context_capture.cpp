// RUN: %clang_cc1 -freflection -std=c++2a -fsyntax-only -verify %s

constexpr auto frag = fragment struct {
  int get_int() const {
    return 0;
  }
};

class Foo {
  consteval {
    -> %{frag}; // expected-error {{the unquote operator can only appear inside of a fragment}}
  }
};
