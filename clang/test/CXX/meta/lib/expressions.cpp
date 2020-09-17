// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

int x = 0;

constexpr meta::info lvalue_refl = reflexpr(x);
static_assert(__reflect(query_is_expression, lvalue_refl));
static_assert(__reflect(query_is_lvalue, lvalue_refl));
static_assert(!__reflect(query_is_xvalue, lvalue_refl));
static_assert(!__reflect(query_is_prvalue, lvalue_refl));
static_assert(!__reflect(query_is_value, lvalue_refl));

struct X {
  int i[2][3];
};

constexpr meta::info xvalue_refl = reflexpr(X().i);
static_assert(__reflect(query_is_expression, xvalue_refl));
static_assert(!__reflect(query_is_lvalue, xvalue_refl));
static_assert(__reflect(query_is_xvalue, xvalue_refl));
static_assert(!__reflect(query_is_prvalue, xvalue_refl));
static_assert(!__reflect(query_is_value, xvalue_refl));

constexpr meta::info prvalue_refl = reflexpr(2 + 2);
static_assert(__reflect(query_is_expression, prvalue_refl));
static_assert(!__reflect(query_is_lvalue, prvalue_refl));
static_assert(!__reflect(query_is_xvalue, prvalue_refl));
static_assert(__reflect(query_is_prvalue, prvalue_refl));
static_assert(!__reflect(query_is_value, prvalue_refl));

constexpr meta::info value_refl = reflexpr(2);
static_assert(__reflect(query_is_expression, value_refl));
static_assert(!__reflect(query_is_lvalue, value_refl));
static_assert(!__reflect(query_is_xvalue, value_refl));
static_assert(__reflect(query_is_prvalue, value_refl));
static_assert(__reflect(query_is_value, value_refl));

constexpr meta::info type_refl = reflexpr(int);
static_assert(!__reflect(query_is_expression, type_refl));
static_assert(!__reflect(query_is_lvalue, type_refl));
static_assert(!__reflect(query_is_xvalue, type_refl));
static_assert(!__reflect(query_is_prvalue, type_refl));
static_assert(!__reflect(query_is_value, type_refl));
