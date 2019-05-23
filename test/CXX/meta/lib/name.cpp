// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

using string_type = const char *;

constexpr bool string_eq(string_type s1, string_type s2) {
  while (*s1 != '\0' && *s1 == *s2) {
    s1++;
    s2++;
  }

  return *s1 == *s2;
}

consteval bool is_named(meta::info reflection) {
  return __reflect(query_is_named, reflection);
}

consteval string_type name_of(meta::info reflection) {
  return __reflect(query_get_name, reflection);
}

namespace namespace_name {}

static_assert(is_named(reflexpr(namespace_name)));
static_assert(string_eq(name_of(reflexpr(namespace_name)), "namespace_name"));

namespace namespace_alias_name = namespace_name;

static_assert(is_named(reflexpr(namespace_alias_name)));
static_assert(string_eq(name_of(reflexpr(namespace_alias_name)), "namespace_alias_name"));

static_assert(is_named(reflexpr(int)));
static_assert(string_eq(name_of(reflexpr(int)), "int"));

using builtin_type_alias_name = int;

static_assert(is_named(reflexpr(builtin_type_alias_name)));
static_assert(string_eq(name_of(reflexpr(builtin_type_alias_name)), "builtin_type_alias_name"));

class type_name;

static_assert(is_named(reflexpr(type_name)));
static_assert(string_eq(name_of(reflexpr(type_name)), "type_name"));

using type_alias_name = type_name;

static_assert(is_named(reflexpr(type_alias_name)));
static_assert(string_eq(name_of(reflexpr(type_alias_name)), "type_alias_name"));

namespace container {
  template<typename T>
  class base_templ_name {
  };

  template<typename T>
  using alias_templ_name = base_templ_name<T>;
}

constexpr meta::info base_templ_refl = __reflect(query_get_begin_member, reflexpr(container));

static_assert(is_named(reflexpr(base_templ_refl)));
static_assert(string_eq(name_of(base_templ_refl), "base_templ_name"));

constexpr meta::info alias_templ_refl = __reflect(query_get_next_member, base_templ_refl);

static_assert(is_named(reflexpr(alias_templ_refl)));
static_assert(string_eq(name_of(alias_templ_refl), "alias_templ_name"));
