// RUN: %clang_cc1 -freflection -std=c++2a %s

#include "../reflection_query.h"

struct Existing {
  Existing(int a) { }
  Existing(float a) { }

  operator int() {
    return 0;
  }

  operator float() {
    return 0;
  }
};

struct New {
  consteval {
    // Constructors
    auto ctor_1 = __reflect(query_get_begin_member, reflexpr(Existing));
    __reflect_mod(query_set_add_explicit, ctor_1, true);

    -> ctor_1;

    auto ctor_2 = __reflect(query_get_next_member, ctor_1);
    __reflect_mod(query_set_add_explicit, ctor_2, true);
    __reflect_mod(query_set_add_explicit, ctor_2, false);

    -> ctor_2;

    // Conversions
    auto conv_1 = __reflect(query_get_next_member, ctor_2);
    __reflect_mod(query_set_add_explicit, conv_1, true);

    -> conv_1;


    auto conv_2 = __reflect(query_get_next_member, conv_1);
    __reflect_mod(query_set_add_explicit, conv_2, true);
    __reflect_mod(query_set_add_explicit, conv_2, false);

    -> conv_2;
  }
};

// Constructors
constexpr auto ctor_1 = __reflect(query_get_begin_member, reflexpr(New));
static_assert(__reflect(query_is_explicit, ctor_1));

constexpr auto ctor_2 = __reflect(query_get_next_member, ctor_1);
static_assert(!__reflect(query_is_explicit, ctor_2));

// Conversions
constexpr auto conv_1 = __reflect(query_get_next_member, ctor_2);
static_assert(__reflect(query_is_explicit, conv_1));

constexpr auto conv_2 = __reflect(query_get_next_member, conv_1);
static_assert(!__reflect(query_is_explicit, conv_2));

int main () {
  return 0;
}
