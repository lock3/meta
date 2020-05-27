// RUN: %clang_cc1 -freflection -Wno-deprecated-fragment -verify -std=c++2a %s

class foo {
  consteval -> __fragment struct {
    constexpr static int anon_x = 10;
  };
  consteval -> __fragment struct foo {
    constexpr static int named_x = 10;
  };

  class {
    consteval -> __fragment struct {
      constexpr static int anon_x = 10; // expected-error {{static data member 'anon_x' not allowed in anonymous class}}
    };
    consteval -> __fragment struct foo {
      constexpr static int named_x = 10; // expected-error {{static data member 'named_x' not allowed in anonymous class}}
    };
  } x;
};
