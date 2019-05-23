// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

void foo();

static_assert(__reflect(query_is_function, reflexpr(foo)));

int x;

static_assert(!__reflect(query_is_function, reflexpr(x)));
