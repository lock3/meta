// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

namespace namespace_name {}
namespace namespace_alias_name = namespace_name;

static_assert(!__reflect(query_is_namespace_alias, reflexpr(namespace_name)));
static_assert(__reflect(query_is_namespace_alias, reflexpr(namespace_alias_name)));

using builtin_type_alias_name = int;

static_assert(!__reflect(query_is_type_alias, reflexpr(int)));
static_assert(__reflect(query_is_type_alias, reflexpr(builtin_type_alias_name)));

class type_name;
using type_alias_name = type_name;

static_assert(!__reflect(query_is_type_alias, reflexpr(type_name)));
static_assert(__reflect(query_is_type_alias, reflexpr(type_alias_name)));

namespace container {
  template<typename T>
  class base_templ_name {
  };

  template<typename T>
  using alias_templ_name = base_templ_name<T>;
}

constexpr meta::info base_templ_refl = __reflect(query_get_begin_member, reflexpr(container));
constexpr meta::info alias_templ_refl = __reflect(query_get_next_member, base_templ_refl);

static_assert(!__reflect(query_is_alias_template, base_templ_refl));
static_assert(__reflect(query_is_alias_template, alias_templ_refl));
