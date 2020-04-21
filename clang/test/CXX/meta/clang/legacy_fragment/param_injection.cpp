// RUN: %clang_cc1 -freflection -Wno-deprecated-fragment -std=c++2a %s

#include "../reflection_iterator.h"

struct bar {
  void do_thing(int a, int b) {
  }

  void undefined_do_thing(int a, int b);
};

constexpr int add(int a, int b) {
  return a + b;
}

class foo {
  consteval {
    {
      meta::info fn_refl = reflexpr(bar::do_thing);
      meta::range params(fn_refl);

      -> __fragment struct {
        constexpr int new_do_thing(-> params) const {
          return add(unqualid(... params));
        }
      };
    }
    {
      meta::info fn_refl = reflexpr(bar::undefined_do_thing);
      meta::range params(fn_refl);

      -> __fragment struct {
        constexpr int new_undefined_do_thing(-> params) const {
          return add(unqualid(... params));
        }
      };
    }
  }
};

template<int = 0>
class foo_two {
  consteval {
    {
      meta::info fn_refl = reflexpr(bar::do_thing);
      meta::range params(fn_refl);

      -> __fragment struct {
        constexpr int new_do_thing(-> params) const {
          return add(unqualid(... params));
        }
      };
    }
    {
      meta::info fn_refl = reflexpr(bar::undefined_do_thing);
      meta::range params(fn_refl);

      -> __fragment struct {
        constexpr int new_undefined_do_thing(-> params) const {
          return add(unqualid(... params));
        }
      };
    }
  }
};

int main() {
  {
    constexpr foo f;
    static_assert(f.new_do_thing(10, 2) == 12);
    static_assert(f.new_undefined_do_thing(10, 2) == 12);
  }
  {
    constexpr foo_two f;
    static_assert(f.new_do_thing(10, 2) == 12);
    static_assert(f.new_undefined_do_thing(10, 2) == 12);
  }
  return 0;
};
