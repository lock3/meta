// RUN: %clang_cc1 -freflection -std=c++2a %s
// expected-no-diagnostics

#include "../reflection_query.h"

using info = decltype(reflexpr(void));

// add_const, remove_const
constexpr info add_const = __reflect(query_add_const, reflexpr(int));
static_assert(__reflect(query_is_const_type, add_const));
constexpr info remove_const = __reflect(query_remove_const, add_const);
static_assert(!__reflect(query_is_const_type, remove_const));

// add_volatile, remove_volatile
constexpr info add_volatile = __reflect(query_add_volatile, reflexpr(int));
static_assert(__reflect(query_is_volatile_type, add_volatile));
constexpr info remove_volatile = __reflect(query_remove_volatile, add_volatile);
static_assert(!__reflect(query_is_volatile_type, remove_volatile));

// add_pointer, remove_pointer
constexpr info add_ptr = __reflect(query_add_pointer, reflexpr(int));
static_assert(__reflect(query_is_pointer_type, add_ptr));
constexpr info remove_ptr = __reflect(query_remove_pointer, add_ptr);
static_assert(!__reflect(query_is_pointer_type, remove_ptr));

// TODO: make_signed
#if 0
constexpr info make_signed = __reflect(query_make_signed, reflexpr(unsigned int));
static_assert(__reflect(query_is_signed_type, make_signed));
static_assert(!__reflect(query_is_unsigned_type, make_signed));
#endif

// make_unsigned
constexpr info make_unsigned = __reflect(query_make_unsigned, reflexpr(int));
static_assert(__reflect(query_is_unsigned_type, make_unsigned));
static_assert(!__reflect(query_is_signed_type, make_unsigned));

// add_lvalue_reference, add_rvalue_reference
constexpr info add_lref = __reflect(query_add_lvalue_reference, reflexpr(int));
static_assert(__reflect(query_is_lvalue_reference_type, add_lref));
static_assert(!__reflect(query_is_rvalue_reference_type, add_lref));
constexpr info add_rref = __reflect(query_add_rvalue_reference, reflexpr(int));
static_assert(__reflect(query_is_rvalue_reference_type, add_rref));
static_assert(!__reflect(query_is_lvalue_reference_type, add_rref));

// remove_reference
constexpr info remove_lref = __reflect(query_remove_reference, add_lref);
static_assert(!__reflect(query_is_lvalue_reference_type, remove_lref));
constexpr info remove_rref = __reflect(query_remove_reference, add_rref);
static_assert(!__reflect(query_is_rvalue_reference_type, remove_rref));

// add_cv, remove_cv
constexpr info add_cv = __reflect(query_add_cv, reflexpr(int));
static_assert(__reflect(query_is_const_type, add_cv));
static_assert(__reflect(query_is_volatile_type, add_cv));
constexpr info remove_cv = __reflect(query_remove_cv, add_cv);
static_assert(!__reflect(query_is_const_type, remove_cv));
static_assert(!__reflect(query_is_volatile_type, remove_cv));

// remove_cvref
constexpr info cv_rref = __reflect(query_add_rvalue_reference, add_cv);
static_assert(__reflect(query_is_rvalue_reference_type, cv_rref));
constexpr info remove_cvrref = __reflect(query_remove_cvref, cv_rref);
static_assert(!__reflect(query_is_rvalue_reference_type, remove_cvrref));
static_assert(!__reflect(query_is_const_type, remove_cvrref));
static_assert(!__reflect(query_is_volatile_type, remove_cvrref));
