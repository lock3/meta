// RUN: %clang_cc1 -std=c++2a -freflection -Wno-deprecated-fragment %s

#include "../reflection_iterator.h"

consteval void test_metaclass(meta::info source) {
  for (auto method : meta::range(source)) {
    -> __fragment class {
    public:
      constexpr int unqualid(method)() { return 10; }
    };
  }
}

class(test_metaclass) test_class {
  int fn_1();
  int fn_2();
};

int main() {
  static_assert(test_class().fn_1() == 10);
  static_assert(test_class().fn_2() == 10);
  return 0;
}
