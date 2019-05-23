// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

extern void extern_fn();

constexpr meta::info extern_fn_refl = reflexpr(extern_fn);
static_assert(__reflect(query_is_extern_specified, extern_fn_refl));
static_assert(!__reflect(query_is_inline, extern_fn_refl));
static_assert(!__reflect(query_is_inline_specified, extern_fn_refl));
static_assert(!__reflect(query_is_constexpr, extern_fn_refl));
static_assert(!__reflect(query_is_consteval, extern_fn_refl));
static_assert(!__reflect(query_is_final, extern_fn_refl));
static_assert(!__reflect(query_is_defined, extern_fn_refl));
static_assert(!__reflect(query_is_complete, extern_fn_refl));

inline void inline_fn();

constexpr meta::info inline_fn_refl = reflexpr(inline_fn);
static_assert(!__reflect(query_is_extern_specified, inline_fn_refl));
static_assert(__reflect(query_is_inline, inline_fn_refl));
static_assert(__reflect(query_is_inline_specified, inline_fn_refl));
static_assert(!__reflect(query_is_constexpr, inline_fn_refl));
static_assert(!__reflect(query_is_consteval, inline_fn_refl));
static_assert(!__reflect(query_is_final, inline_fn_refl));
static_assert(!__reflect(query_is_defined, inline_fn_refl));
static_assert(!__reflect(query_is_complete, inline_fn_refl));

constexpr void constexpr_fn();

constexpr meta::info constexpr_fn_refl = reflexpr(constexpr_fn);
static_assert(!__reflect(query_is_extern_specified, constexpr_fn_refl));
static_assert(__reflect(query_is_inline, constexpr_fn_refl));
static_assert(!__reflect(query_is_inline_specified, constexpr_fn_refl));
static_assert(__reflect(query_is_constexpr, constexpr_fn_refl));
static_assert(!__reflect(query_is_consteval, constexpr_fn_refl));
static_assert(!__reflect(query_is_final, constexpr_fn_refl));
static_assert(!__reflect(query_is_defined, constexpr_fn_refl));
static_assert(!__reflect(query_is_complete, constexpr_fn_refl));

consteval void consteval_fn();

constexpr meta::info consteval_fn_refl = reflexpr(consteval_fn);
static_assert(!__reflect(query_is_extern_specified, consteval_fn_refl));
static_assert(__reflect(query_is_inline, consteval_fn_refl));
static_assert(!__reflect(query_is_inline_specified, consteval_fn_refl));
static_assert(__reflect(query_is_constexpr, consteval_fn_refl));
static_assert(__reflect(query_is_consteval, consteval_fn_refl));
static_assert(!__reflect(query_is_final, consteval_fn_refl));
static_assert(!__reflect(query_is_defined, consteval_fn_refl));
static_assert(!__reflect(query_is_complete, consteval_fn_refl));

struct base_class {
  virtual void foo();
};

constexpr meta::info base_class_refl = reflexpr(base_class);
static_assert(!__reflect(query_is_extern_specified, base_class_refl));
static_assert(!__reflect(query_is_inline, base_class_refl));
static_assert(!__reflect(query_is_inline_specified, base_class_refl));
static_assert(!__reflect(query_is_consteval, base_class_refl));
static_assert(!__reflect(query_is_consteval, base_class_refl));
static_assert(!__reflect(query_is_final, base_class_refl));
static_assert(!__reflect(query_is_defined, base_class_refl));
static_assert(__reflect(query_is_complete, base_class_refl));

struct child_class final : base_class {
  virtual void foo() final;
};

constexpr meta::info final_child_class_refl = reflexpr(child_class);
static_assert(!__reflect(query_is_extern_specified, final_child_class_refl));
static_assert(!__reflect(query_is_inline, final_child_class_refl));
static_assert(!__reflect(query_is_inline_specified, final_child_class_refl));
static_assert(!__reflect(query_is_consteval, final_child_class_refl));
static_assert(!__reflect(query_is_consteval, final_child_class_refl));
static_assert(__reflect(query_is_final, final_child_class_refl));
static_assert(!__reflect(query_is_defined, final_child_class_refl));
static_assert(__reflect(query_is_complete, final_child_class_refl));

constexpr meta::info final_child_class_foo_refl = __reflect(query_get_begin, final_child_class_refl);
static_assert(!__reflect(query_is_extern_specified, final_child_class_foo_refl));
static_assert(!__reflect(query_is_inline, final_child_class_foo_refl));
static_assert(!__reflect(query_is_inline_specified, final_child_class_foo_refl));
static_assert(!__reflect(query_is_consteval, final_child_class_foo_refl));
static_assert(!__reflect(query_is_consteval, final_child_class_foo_refl));
static_assert(__reflect(query_is_final, final_child_class_foo_refl));
static_assert(!__reflect(query_is_defined, final_child_class_foo_refl));
static_assert(!__reflect(query_is_complete, final_child_class_foo_refl));

void defined_fn() { }

constexpr meta::info defined_fn_refl = reflexpr(defined_fn);
static_assert(!__reflect(query_is_extern_specified, defined_fn_refl));
static_assert(!__reflect(query_is_inline, defined_fn_refl));
static_assert(!__reflect(query_is_inline_specified, defined_fn_refl));
static_assert(!__reflect(query_is_constexpr, defined_fn_refl));
static_assert(!__reflect(query_is_consteval, defined_fn_refl));
static_assert(!__reflect(query_is_final, defined_fn_refl));
static_assert(__reflect(query_is_defined, defined_fn_refl));
static_assert(!__reflect(query_is_complete, defined_fn_refl));
