// RUN: %clang_cc1 -freflection -Wno-deprecated-fragment -std=c++2a %s

#include "../reflection_query.h"

constexpr int x = 10;
constexpr int y = 20;

class z {
  consteval ->  __fragment struct k {
    static constexpr int member_x = x;
    constexpr int foo_0() { return x + member_x; }
    consteval ->  __fragment struct k2 {
      static constexpr int member_y = y;
      constexpr int foo_1() { return y + member_y; }
    };
  };

  consteval -> __fragment class k {
    static constexpr auto frag = __fragment struct {
      constexpr int foo_2() { return 2; }
    };

    consteval -> __fragment class k2 {
      consteval -> frag;
    };
  };
};

consteval {
  (void) __reflect_pretty_print(__reflect(query_get_definition, reflexpr(z)));
}


int main() {
  z i_z;
  static_assert(i_z.member_x == x);
  static_assert(i_z.foo_0() == x * 2);
  static_assert(i_z.member_y == y);
  static_assert(i_z.foo_1() == y * 2);
  static_assert(i_z.foo_2() == 2);
  return 0;
}
