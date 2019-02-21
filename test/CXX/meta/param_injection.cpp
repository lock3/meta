// RUN: %clang_cc1 -freflection -std=c++1z %s

#include "reflection_iterator.h"

struct bar {
  void do_thing(int a, int b) {
  }
};

constexpr int add(int a, int b) {
  return a + b;
}

class foo {
  consteval {
    meta::info bar_do_thing_refl = reflexpr(bar::do_thing);
    member_range params(bar_do_thing_refl);

    -> __fragment struct {
      constexpr int new_do_thing(-> params) const {
        return add(unqualid(... params));
      }
    };
  }
};

int main() {
  constexpr foo f;
  static_assert(f.new_do_thing(10, 2) == 12);
  return 0;
};
