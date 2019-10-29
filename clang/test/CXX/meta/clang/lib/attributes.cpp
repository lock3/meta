// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

void no_attr_fn();

constexpr meta::info no_attr_fn_refl = reflexpr(no_attr_fn);
static_assert(!__reflect(query_has_attribute, no_attr_fn_refl, "noreturn"));

[[noreturn]] void fn();

constexpr meta::info fn_refl = reflexpr(fn);
static_assert(__reflect(query_has_attribute, fn_refl, "noreturn"));
static_assert(!__reflect(query_has_attribute, fn_refl, "deprecated"));

consteval bool indirect_has_attribute(meta::info fn_refl, const char *attribute_name) {
  return __reflect(query_has_attribute, fn_refl, attribute_name);
}

static_assert(indirect_has_attribute(fn_refl, "noreturn"));
static_assert(!indirect_has_attribute(fn_refl, "deprecated"));
