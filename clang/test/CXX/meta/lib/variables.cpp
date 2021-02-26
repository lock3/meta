// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

int x;

static_assert(__reflect(query_is_variable, ^x));
static_assert(__reflect(query_has_static_storage, ^x));
static_assert(!__reflect(query_has_thread_local_storage, ^x));
static_assert(!__reflect(query_has_automatic_storage, ^x));

thread_local int k;

static_assert(__reflect(query_is_variable, ^k));
static_assert(!__reflect(query_has_static_storage, ^k));
static_assert(__reflect(query_has_thread_local_storage, ^k));
static_assert(!__reflect(query_has_automatic_storage, ^k));

void local() {
  int y;

  static_assert(__reflect(query_is_variable, ^y));
  static_assert(!__reflect(query_has_static_storage, ^y));
  static_assert(!__reflect(query_has_thread_local_storage, ^y));
  static_assert(__reflect(query_has_automatic_storage, ^y));
}

static_assert(!__reflect(query_is_variable, ^local));
