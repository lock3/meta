// RUN: %clang_cc1 -freflection -std=c++1z %s

#include "reflection_query.h"
#include "reflection_mod.h"

class Existing {
  int field_1 = 1;
  int field_2 = 2;
};

struct New {
  constexpr {
    auto refl = reflexpr(Existing);

    auto field_1 = __reflect(query_get_begin, refl);
    __reflect_mod(query_set_access, field_1, AccessModifier::Public);

    -> field_1;

    auto field_2 = __reflect(query_get_next, field_1);
    __reflect_mod(query_set_access, field_2, AccessModifier::Default);

    -> field_2;
  }

public:
  constexpr New() { }
};

int main() {
  constexpr New n;
  static_assert(n.field_1 == 1);
  static_assert(n.field_2 == 2);
  return 0;
}
