// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

int x;

static_assert(__reflect(query_is_variable, reflexpr(x)));
static_assert(__reflect(query_has_static_storage, reflexpr(x)));
static_assert(!__reflect(query_has_thread_local_storage, reflexpr(x)));
static_assert(!__reflect(query_has_automatic_storage, reflexpr(x)));

thread_local int k;

static_assert(__reflect(query_is_variable, reflexpr(k)));
static_assert(!__reflect(query_has_static_storage, reflexpr(k)));
static_assert(__reflect(query_has_thread_local_storage, reflexpr(k)));
static_assert(!__reflect(query_has_automatic_storage, reflexpr(k)));

void local() {
  int y;

  static_assert(__reflect(query_is_variable, reflexpr(y)));
  static_assert(!__reflect(query_has_static_storage, reflexpr(y)));
  static_assert(!__reflect(query_has_thread_local_storage, reflexpr(y)));
  static_assert(__reflect(query_has_automatic_storage, reflexpr(y)));
}

static_assert(!__reflect(query_is_variable, reflexpr(local)));
