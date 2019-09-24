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

  class child_class : base_templ_name<int> {
  };
}

constexpr meta::info base_templ_refl = __reflect(query_get_begin_member, reflexpr(container));

static_assert(is_named(base_templ_refl));
static_assert(string_eq(name_of(base_templ_refl), "base_templ_name"));

constexpr meta::info alias_templ_refl = __reflect(query_get_next_member, base_templ_refl);

static_assert(is_named(alias_templ_refl));
static_assert(string_eq(name_of(alias_templ_refl), "alias_templ_name"));

constexpr meta::info child_class_refl = __reflect(query_get_next_member, alias_templ_refl);

static_assert(is_named(child_class_refl));
static_assert(string_eq(name_of(child_class_refl), "child_class"));

constexpr meta::info parent_base_refl = __reflect(query_get_begin_base_spec, child_class_refl);

static_assert(is_named(parent_base_refl));
static_assert(string_eq(name_of(parent_base_refl), "base_templ_name"));

struct class_with_named_special_members {
  class_with_named_special_members();
  ~class_with_named_special_members();

  int operator++();
};

constexpr meta::info constructor_refl = __reflect(query_get_begin_member, reflexpr(class_with_named_special_members));

static_assert(is_named(constructor_refl));
static_assert(string_eq(name_of(constructor_refl), "class_with_named_special_members"));

constexpr meta::info destructor_refl = __reflect(query_get_next_member, constructor_refl);

static_assert(is_named(destructor_refl));
static_assert(string_eq(name_of(destructor_refl), "~class_with_named_special_members"));

constexpr meta::info operator_refl = __reflect(query_get_next_member, destructor_refl);

static_assert(is_named(operator_refl));
static_assert(string_eq(name_of(operator_refl), "operator++"));
