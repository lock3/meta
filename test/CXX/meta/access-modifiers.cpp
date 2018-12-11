// RUN: %clang_cc1 -freflection -std=c++1z %s

#include "reflection_query.h"
#include "reflection_mod.h"

class Existing {
  int foo = 1;
};

class New {
  constexpr {
    auto refl = reflexpr(Existing);

    auto field_1 = __reflect(query_get_begin, refl);
    __reflect_mod(query_set_access, field_1, AccessModifier::Public);

    -> field_1;
  }

public:
  constexpr New() { }
};

int main() {
  constexpr New n;
  static_assert(n.foo == 1);
  return 0;
}
