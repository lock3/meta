// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

enum E {};

static_assert(__reflect(query_is_unscoped_enum, ^E));
static_assert(!__reflect(query_is_scoped_enum, ^E));

enum class EC {};

static_assert(!__reflect(query_is_unscoped_enum, ^EC));
static_assert(__reflect(query_is_scoped_enum, ^EC));
