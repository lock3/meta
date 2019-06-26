// RUN: %clang_cc1 -std=c++2a -freflection %s
// expected_no_diagnostics

#include "../reflection_query.h"

int x = 42;
static_assert(__reflect(query_is_lvalue, reflexpr(x)));
static_assert(!__reflect(query_is_rvalue, reflexpr(x)));

void test() {
  struct S {
    int f;
  };

  static_assert(__reflect(query_is_xvalue, reflexpr(S().f)));
}
