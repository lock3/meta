// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

class Foo {
  static int x;
  int y;
  unsigned z : 16;
  mutable int a;
};

constexpr meta::info field_1 = __reflect(query_get_begin, reflexpr(Foo));
static_assert(__reflect(query_is_static_data_member, field_1));
static_assert(!__reflect(query_is_nonstatic_data_member, field_1));
static_assert(!__reflect(query_is_bit_field, field_1));
static_assert(!__reflect(query_is_mutable, field_1));

constexpr meta::info field_2 = __reflect(query_get_next, field_1);
static_assert(!__reflect(query_is_static_data_member, field_2));
static_assert(__reflect(query_is_nonstatic_data_member, field_2));
static_assert(!__reflect(query_is_bit_field, field_2));
static_assert(!__reflect(query_is_mutable, field_2));

constexpr meta::info field_3 = __reflect(query_get_next, field_2);
static_assert(!__reflect(query_is_static_data_member, field_3));
static_assert(__reflect(query_is_nonstatic_data_member, field_3));
static_assert(__reflect(query_is_bit_field, field_3));
static_assert(!__reflect(query_is_mutable, field_3));

constexpr meta::info field_4 = __reflect(query_get_next, field_3);
static_assert(!__reflect(query_is_static_data_member, field_4));
static_assert(__reflect(query_is_nonstatic_data_member, field_4));
static_assert(!__reflect(query_is_bit_field, field_4));
static_assert(__reflect(query_is_mutable, field_4));
