// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

namespace Foo { }

constexpr meta::info namespace_refl = reflexpr(Foo);
static_assert(__reflect(query_is_namespace, namespace_refl));

class Bar;

constexpr meta::info class_refl = reflexpr(Bar);
static_assert(!__reflect(query_is_namespace, class_refl));
