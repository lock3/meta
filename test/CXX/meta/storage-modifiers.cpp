// RUN: %clang_cc1 -freflection -std=c++1z %s

#include "reflection_query.h"
#include "reflection_mod.h"

struct Existing {
  int field_1;
};

struct New {
  consteval {
    auto refl = reflexpr(Existing);

    // Fields
    auto field_1 = __reflect(query_get_begin, refl);
    __reflect_mod(query_set_storage, field_1, StorageModifier::Static);

    -> field_1;
  }

public:
  constexpr New() { }
};

int main() {
  // Fields
  New::field_1 = 0;

  return 0;
}
