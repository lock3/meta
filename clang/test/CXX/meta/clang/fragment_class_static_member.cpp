// RUN: %clang_cc1 -freflection -verify -std=c++2a %s

class foo {
  consteval -> fragment struct {
    constexpr static int anon_x = 10;
  };
  consteval -> fragment struct foo {
    constexpr static int named_x = 10;
  };

  class {
    consteval -> fragment struct {
      constexpr static int anon_x = 10; // expected-error {{static data member 'anon_x' not allowed in anonymous class}}
    };
    consteval -> fragment struct foo {
      constexpr static int named_x = 10; // expected-error {{static data member 'named_x' not allowed in anonymous class}}
    };
  } x;
};
