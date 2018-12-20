// RUN: %clang_cc1 -freflection -std=c++1z %s

#include "reflection_query.h"
#include "reflection_mod.h"

// template<int a>
// constexpr int template_a() {
//   return a;
// }

class Existing {
  int field_1 = 1;
  int field_2 = 2;

  constexpr int method_1() const {
    return 1;
  }

  constexpr int method_2() const {
    return 2;
  }

  typedef int typedef_1;
  typedef int typedef_2;
};

struct New {
  consteval {
    auto refl = reflexpr(Existing);

    // Fields
    auto field_1 = __reflect(query_get_begin, refl);
    __reflect_mod(query_set_access, field_1, AccessModifier::Public);

    -> field_1;

    auto field_2 = __reflect(query_get_next, field_1);
    __reflect_mod(query_set_access, field_2, AccessModifier::Default);

    -> field_2;

    // Methods
    auto method_1 = __reflect(query_get_next, field_2);
    __reflect_mod(query_set_access, method_1, AccessModifier::Public);

    -> method_1;

    auto method_2 = __reflect(query_get_next, method_1);
    __reflect_mod(query_set_access, method_2, AccessModifier::Default);

    -> method_2;

    // Typedef
    auto typedef_1 = __reflect(query_get_next, method_2);
    __reflect_mod(query_set_access, typedef_1, AccessModifier::Public);

    -> typedef_1;

    auto typedef_2 = __reflect(query_get_next, typedef_1);
    __reflect_mod(query_set_access, typedef_2, AccessModifier::Default);

    -> typedef_2;

    // Template Function
    // auto template_refl_a = reflexpr(template_a);
    // __reflect_mod(query_set_access, template_refl_a, AccessModifier::Public);

    // -> template_refl_a;
  }

public:
  constexpr New() { }
};

int main() {
  constexpr New n;

  // Fields
  static_assert(n.field_1 == 1);
  static_assert(n.field_2 == 2);

  // Methods
  static_assert(n.method_1() == 1);
  static_assert(n.method_2() == 2);

  // Typdefs
  New::typedef_1 typedef_1_val = 0;
  New::typedef_2 typedef_2_val = 0;

  return 0;
}
