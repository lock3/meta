// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

namespace foo { }

constexpr meta::info namespace_refl = ^foo;
static_assert(__reflect(query_is_namespace, namespace_refl));

namespace foo_alias = foo;

constexpr meta::info namespace_alias_refl = ^foo_alias;
static_assert(__reflect(query_is_namespace, namespace_alias_refl));

class bar;

constexpr meta::info class_refl = ^bar;
static_assert(!__reflect(query_is_namespace, class_refl));
