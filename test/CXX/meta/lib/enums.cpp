// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

enum E {};

static_assert(__reflect(query_is_unscoped_enum, reflexpr(E)));
static_assert(!__reflect(query_is_scoped_enum, reflexpr(E)));

enum class EC {};

static_assert(!__reflect(query_is_unscoped_enum, reflexpr(EC)));
static_assert(__reflect(query_is_scoped_enum, reflexpr(EC)));
