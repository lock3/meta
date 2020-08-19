// Run via frontend, to verify linker finds get_inline_value.
// RUN: %clang -freflection -std=c++2a %s

#include "reflection_iterator.h"

consteval void interface(meta::info source) {
  int default_val = 1;
  -> fragment struct {
    int val = %{default_val};

    constexpr void set_foo(const int& val) {
      this->val = val;
    }

    constexpr int foo() const {
      return val;
    }

    constexpr void reset_foo() {
      set_foo(%{default_val});
    }
  };

  -> fragment struct {
    int dedicated_field = 0;
  };

  for (meta::info mem : meta::range(source)) {
    -> mem;
  }
};

class(interface) Thing {
  int inline_value;

public:
  constexpr Thing(int inline_value = 3) : inline_value{inline_value} { }

  constexpr int get_inline_value() const { return inline_value; }
};

// Test #1

consteval {
  constexpr Thing thing;
  static_assert(thing.foo() == 1);
}

// Test #2

constexpr Thing test_2_val() {
  Thing thing;
  thing.set_foo(2);
  return thing;
}

consteval {
  constexpr Thing thing = test_2_val();
  static_assert(thing.foo() == 2);
}

// Test #3

constexpr Thing test_3_val() {
  Thing thing;
  thing.set_foo(2);
  thing.reset_foo();
  return thing;
}

consteval {
  constexpr Thing thing = test_3_val();
  static_assert(thing.foo() == 1);
}

// Test #4

consteval {
  constexpr Thing thing;
  static_assert(thing.get_inline_value() == 3);
}

int main() {
  return 0;
}
