// RUN: %clang_cc1 -freflection -std=c++2a %s

#include "reflection_query.h"
#include "reflection_mod.h"

struct Existing {
  int field_1 = 1;

  constexpr int method_1() const {
    return 1;
  }
};

struct New {
  consteval {
    // Fields
    auto field_1 = reflexpr(Existing::field_1);
    __reflect_mod(query_set_new_name, field_1, "ns_field_1");

    -> field_1;

    // Methods
    auto method_1 = reflexpr(Existing::method_1);
    __reflect_mod(query_set_new_name, method_1, "ns_method_1");

    -> method_1;
  }

  constexpr New() { }
};

int main() {
  constexpr New n;

  // Fields
  static_assert(n.ns_field_1 == 1);

  // Methods
  static_assert(n.ns_method_1() == 1);

  return 0;
}
