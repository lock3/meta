// RUN: %clang_cc1 -freflection -std=c++1z %s

#include "reflection_iterator.h"

struct bar {
  void do_thing(int a, int b) {
  }
};

class foo {
  consteval {
    meta::info bar_do_thing_refl = reflexpr(bar::do_thing);

    -> __fragment struct {
      constexpr int new_do_thing(-> member_range(bar_do_thing_refl)) const {
        return unqualid(*(member_range(bar_do_thing_refl).begin()));
      }
    };
  }
};

int main() {
  constexpr foo f;
  static_assert(f.new_do_thing(10, 2) == 10);
  return 0;
};
