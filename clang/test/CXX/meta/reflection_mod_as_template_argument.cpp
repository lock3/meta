// RUN: %clang_cc1 -freflection -std=c++2a %s

#include "reflection_query.h"
#include "reflection_mod.h"

struct Existing {
  int field_1 = 1;
};

template<meta::info V>
struct New {
  consteval {
    -> V;
  }

  constexpr New() { }
};

consteval auto get_field_1() {
  auto field_1 = reflexpr(Existing::field_1);
  __reflect_mod(query_set_new_name, field_1, "ns_field_1");
  return field_1;
}

int main() {
  constexpr New<get_field_1()> n;
  static_assert(n.ns_field_1 == 1);

  return 0;
}
