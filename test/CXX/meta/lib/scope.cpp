// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

int global_var;

constexpr meta::info global_var_refl = reflexpr(global_var);
static_assert(!__reflect(query_is_local, global_var_refl));

void foo() {
  int local_var = 0;

  constexpr meta::info local_var_refl = reflexpr(local_var);
  static_assert(__reflect(query_is_local, local_var_refl));
}

struct wrapper_class {
  int var;
  void fn();
  using alias = int;
};

constexpr meta::info class_member_var_refl = reflexpr(wrapper_class::var);
static_assert(!__reflect(query_is_local, class_member_var_refl));

constexpr meta::info class_member_fn_refl = reflexpr(wrapper_class::fn);
static_assert(!__reflect(query_is_local, class_member_fn_refl));

constexpr meta::info class_type_alias_refl = reflexpr(wrapper_class::alias);
static_assert(!__reflect(query_is_local, class_type_alias_refl));
