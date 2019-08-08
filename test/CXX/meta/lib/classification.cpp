// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

constexpr meta::info invalid_reflection() {
  return __invalid_reflection("");
}

static_assert(__reflect(query_is_invalid, invalid_reflection()));

constexpr meta::info valid_reflection() {
  return reflexpr(int);
}
static_assert(!__reflect(query_is_invalid, valid_reflection()));
