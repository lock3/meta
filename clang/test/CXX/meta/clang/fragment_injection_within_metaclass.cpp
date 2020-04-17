// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "reflection_iterator.h"

consteval void test_metaclass(meta::info source) {
  for (auto method : meta::range(source)) {
    -> method;
  }
}

class base_test_class {
  int a;
  int b;
};

consteval void do_thing() {
  for (auto field : meta::range(reflexpr(base_test_class))) {
    -> fragment class {
        public:
        constexpr int unqualid("fn_", %{field})() { return 10; }
    };
  }
}

class(test_metaclass) test_class {
  consteval {
    do_thing();
  }
};

int main() {
  static_assert(test_class().fn_a() == 10);
  static_assert(test_class().fn_b() == 10);
  return 0;
}
