// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

enum E {
  E_val
};

static_assert(__reflect(query_is_enumerator, ^E_val));
static_assert(!__reflect(query_is_enumerator, ^E));

enum class EC {
  val
};

static_assert(__reflect(query_is_enumerator, ^EC::val));
static_assert(!__reflect(query_is_enumerator, ^EC));
