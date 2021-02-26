// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "reflection_iterator.h"
#include "reflection_query.h"

consteval auto name_of(meta::info refl) {
  return __reflect(query_get_name, refl);
}

consteval void test_metaclass(meta::info source) {
  for (auto method : meta::range(source)) {
    -> fragment class {
    public:
      constexpr int [# name_of(%{method}) #]() { return 10; }
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
