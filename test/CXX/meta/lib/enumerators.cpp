// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

enum E {
  E_val
};

static_assert(__reflect(query_is_enumerator, reflexpr(E_val)));
static_assert(!__reflect(query_is_enumerator, reflexpr(E)));

enum class EC {
  val
};

static_assert(__reflect(query_is_enumerator, reflexpr(EC::val)));
static_assert(!__reflect(query_is_enumerator, reflexpr(EC)));
