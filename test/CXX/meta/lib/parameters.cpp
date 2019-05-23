// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

void foo(int a, int b = 0) { }

constexpr auto function_refl = reflexpr(foo);

constexpr auto param_a_refl = __reflect(query_get_begin, function_refl);
static_assert(__reflect(query_is_function_parameter, param_a_refl));
static_assert(!__reflect(query_is_template_parameter, param_a_refl));
static_assert(!__reflect(query_is_type_template_parameter, param_a_refl));
static_assert(!__reflect(query_is_nontype_template_parameter, param_a_refl));
static_assert(!__reflect(query_is_template_template_parameter, param_a_refl));
static_assert(!__reflect(query_has_default_argument, param_a_refl));

constexpr auto param_b_refl = __reflect(query_get_next, param_a_refl);
static_assert(__reflect(query_is_function_parameter, param_b_refl));
static_assert(!__reflect(query_is_template_parameter, param_b_refl));
static_assert(!__reflect(query_is_type_template_parameter, param_b_refl));
static_assert(!__reflect(query_is_nontype_template_parameter, param_b_refl));
static_assert(!__reflect(query_is_template_template_parameter, param_b_refl));
static_assert(__reflect(query_has_default_argument, param_b_refl));
