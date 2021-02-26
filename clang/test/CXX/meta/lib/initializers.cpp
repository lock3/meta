// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

int x;

static_assert(!__reflect(query_has_initializer, ^x));

int x_init = 0;

static_assert(__reflect(query_has_initializer, ^x_init));

thread_local int k;

static_assert(!__reflect(query_has_initializer, ^k));

thread_local int k_init = 0;

static_assert(__reflect(query_has_initializer, ^k_init));


void local() {
  int y;

  static_assert(!__reflect(query_has_initializer, ^y));

  int y_init = 0;

  static_assert(__reflect(query_has_initializer, ^y_init));
}

class clazz {
  int z;
  int z_init = 0;

  static_assert(!__reflect(query_has_initializer, ^z));
  static_assert(__reflect(query_has_initializer, ^z_init));
};
