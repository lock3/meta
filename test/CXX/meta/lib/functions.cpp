// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

void foo();

constexpr meta::info function_refl = reflexpr(foo);
static_assert(__reflect(query_is_function, function_refl));
static_assert(!__reflect(query_is_noexcept, function_refl));

void noexcept_foo() noexcept;

constexpr meta::info noexcept_function_refl = reflexpr(noexcept_foo);
static_assert(__reflect(query_is_function, noexcept_function_refl));
static_assert(__reflect(query_is_noexcept, noexcept_function_refl));

int x;

constexpr meta::info var_refl = reflexpr(x);
static_assert(!__reflect(query_is_function, var_refl));
