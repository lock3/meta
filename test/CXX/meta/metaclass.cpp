// Run via frontend, to verify linker finds get_inline_value.
// RUN: %clang -freflection -std=c++1z %s

#include "reflection_iterator.h"

#define assert(E) if (!(E)) __builtin_abort();

constexpr void interface(meta::info source) {
  int default_val = 1;
  -> __fragment struct {
    int val = default_val;

    void set_foo(const int& val) {
      this->val = val;
    }

    int foo() {
      return val;
    }

    void reset_foo() {
      set_foo(default_val);
    }
  };

  -> __fragment struct {
    int dedicated_field;
  };

  for (meta::info mem : member_range(source)) {
    -> mem;
  }
};

class(interface) Thing {
public:
  int inline_value = 3;

  int get_inline_value() { return inline_value; }
};

int main() {
  Thing thing;

  assert(thing.foo() == 1);

  thing.set_foo(2);
  assert(thing.foo() == 2);

  thing.reset_foo();
  assert(thing.foo() == 1);

  assert(thing.get_inline_value() == 3);

  return 0;
}
