// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

void foo(int a, int b = 0) { }

constexpr auto function_refl = reflexpr(foo);

constexpr auto param_a_refl = __reflect(query_get_begin_param, function_refl);
static_assert(__reflect(query_is_function_parameter, param_a_refl));
static_assert(!__reflect(query_is_type_template_parameter, param_a_refl));
static_assert(!__reflect(query_is_nontype_template_parameter, param_a_refl));
static_assert(!__reflect(query_is_template_template_parameter, param_a_refl));
static_assert(!__reflect(query_has_default_argument, param_a_refl));

constexpr auto param_b_refl = __reflect(query_get_next_param, param_a_refl);
static_assert(__reflect(query_is_function_parameter, param_b_refl));
static_assert(!__reflect(query_is_type_template_parameter, param_b_refl));
static_assert(!__reflect(query_is_nontype_template_parameter, param_b_refl));
static_assert(!__reflect(query_is_template_template_parameter, param_b_refl));
static_assert(__reflect(query_has_default_argument, param_b_refl));

template<typename T>
class container {
};

namespace templ_ns {
  template<typename A, typename B = int>
  void templ_type_fn() { }

  template<int A, int B = 0>
  void templ_nontype_fn() { }

  template<template<typename> typename A, template<typename> typename B = container>
  void templ_templ_fn() { }
}

constexpr auto templ_type_fn_refl = __reflect(query_get_begin_member, reflexpr(templ_ns));

namespace templ_ns_fn_1 {
  constexpr auto templ_param_a_refl = __reflect(query_get_begin_template_param, templ_type_fn_refl);
  static_assert(!__reflect(query_is_function_parameter, templ_param_a_refl));
  static_assert(__reflect(query_is_type_template_parameter, templ_param_a_refl));
  static_assert(!__reflect(query_is_nontype_template_parameter, templ_param_a_refl));
  static_assert(!__reflect(query_is_template_template_parameter, templ_param_a_refl));
  static_assert(!__reflect(query_has_default_argument, templ_param_a_refl));

  constexpr auto templ_param_b_refl = __reflect(query_get_next_template_param, templ_param_a_refl);
  static_assert(!__reflect(query_is_function_parameter, templ_param_b_refl));
  static_assert(__reflect(query_is_type_template_parameter, templ_param_b_refl));
  static_assert(!__reflect(query_is_nontype_template_parameter, templ_param_b_refl));
  static_assert(!__reflect(query_is_template_template_parameter, templ_param_b_refl));
  static_assert(__reflect(query_has_default_argument, templ_param_b_refl));
}

constexpr auto templ_nontype_fn_refl = __reflect(query_get_next_member, templ_type_fn_refl);

namespace templ_ns_fn_2 {
  constexpr auto templ_param_a_refl = __reflect(query_get_begin_template_param, templ_nontype_fn_refl);
  static_assert(!__reflect(query_is_function_parameter, templ_param_a_refl));
  static_assert(!__reflect(query_is_type_template_parameter, templ_param_a_refl));
  static_assert(__reflect(query_is_nontype_template_parameter, templ_param_a_refl));
  static_assert(!__reflect(query_is_template_template_parameter, templ_param_a_refl));
  static_assert(!__reflect(query_has_default_argument, templ_param_a_refl));

  constexpr auto templ_param_b_refl = __reflect(query_get_next_template_param, templ_param_a_refl);
  static_assert(!__reflect(query_is_function_parameter, templ_param_b_refl));
  static_assert(!__reflect(query_is_type_template_parameter, templ_param_b_refl));
  static_assert(__reflect(query_is_nontype_template_parameter, templ_param_b_refl));
  static_assert(!__reflect(query_is_template_template_parameter, templ_param_b_refl));
  static_assert(__reflect(query_has_default_argument, templ_param_b_refl));
}

constexpr auto templ_templ_fn_refl = __reflect(query_get_next_member, templ_nontype_fn_refl);

namespace templ_ns_fn_3 {
  constexpr auto templ_param_a_refl = __reflect(query_get_begin_template_param, templ_templ_fn_refl);
  static_assert(!__reflect(query_is_function_parameter, templ_param_a_refl));
  static_assert(!__reflect(query_is_type_template_parameter, templ_param_a_refl));
  static_assert(!__reflect(query_is_nontype_template_parameter, templ_param_a_refl));
  static_assert(__reflect(query_is_template_template_parameter, templ_param_a_refl));
  static_assert(!__reflect(query_has_default_argument, templ_param_a_refl));

  constexpr auto templ_param_b_refl = __reflect(query_get_next_template_param, templ_param_a_refl);
  static_assert(!__reflect(query_is_function_parameter, templ_param_b_refl));
  static_assert(!__reflect(query_is_type_template_parameter, templ_param_b_refl));
  static_assert(!__reflect(query_is_nontype_template_parameter, templ_param_b_refl));
  static_assert(__reflect(query_is_template_template_parameter, templ_param_b_refl));
  static_assert(__reflect(query_has_default_argument, templ_param_b_refl));
}
