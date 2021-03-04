// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

constexpr meta::info expr_refl = ^(2 + 2);
static_assert(expr_refl != ^int);

constexpr meta::info expr_type_refl = __reflect(query_get_type, expr_refl);
static_assert(expr_type_refl == ^int);

namespace type_decl_container {
  using special_type = int;
}

constexpr meta::info type_decl_refl = __reflect(query_get_begin, ^type_decl_container);
static_assert(type_decl_refl != ^int);

constexpr meta::info type_decl_type_refl = __reflect(query_get_type, type_decl_refl);
static_assert(type_decl_type_refl == ^int);

namespace value_decl_container {
  int value_decl = 10;
}

constexpr meta::info value_decl_refl = __reflect(query_get_begin, ^value_decl_container);
static_assert(value_decl_refl != ^int);

constexpr meta::info value_decl_type_refl = __reflect(query_get_type, value_decl_refl);
static_assert(value_decl_type_refl == ^int);

int function() { return 0; }

constexpr meta::info function_decl_refl = ^function;
static_assert(function_decl_refl != ^int);

constexpr meta::info function_decl_return_type_refl = __reflect(query_get_return_type, function_decl_refl);
static_assert(function_decl_return_type_refl == ^int);

using function_type = int (*)(float);

constexpr meta::info function_pointer_type_refl = ^function_type;
static_assert(function_pointer_type_refl != ^int);

constexpr meta::info function_type_refl = __reflect(query_remove_pointer, function_pointer_type_refl);
static_assert(function_type_refl != ^int);

constexpr meta::info function_type_return_type_refl = __reflect(query_get_return_type, function_type_refl);
static_assert(function_type_return_type_refl == ^int);

struct clazz {
  void method() { }

  void const_method() const { }
};

constexpr meta::info method_refl = ^clazz::method;
static_assert(method_refl != ^clazz);

constexpr meta::info method_this_type_refl = __reflect(query_get_this_ref_type, method_refl);
static_assert(method_this_type_refl == ^clazz*);

constexpr meta::info const_method_refl = ^clazz::const_method;
static_assert(const_method_refl != ^clazz);

constexpr meta::info const_method_this_type_refl = __reflect(query_get_this_ref_type, const_method_refl);
static_assert(const_method_this_type_refl == ^const clazz*);

enum unspecified_enum { };

constexpr meta::info unspecified_enum_refl = ^unspecified_enum;
static_assert(unspecified_enum_refl != ^unsigned);

constexpr meta::info unspecified_enum_refl_underlying_type = __reflect(query_get_underlying_type, unspecified_enum_refl);
static_assert(unspecified_enum_refl_underlying_type == ^unsigned);

enum specified_enum : int { };

constexpr meta::info specified_enum_refl = ^specified_enum;
static_assert(specified_enum_refl != ^int);

constexpr meta::info specified_enum_refl_underlying_type = __reflect(query_get_underlying_type, specified_enum_refl);
static_assert(specified_enum_refl_underlying_type == ^int);

enum class unspecified_enum_class { };

constexpr meta::info unspecified_enum_class_refl = ^unspecified_enum_class;
static_assert(unspecified_enum_class_refl != ^int);

constexpr meta::info unspecified_enum_class_refl_underlying_type = __reflect(query_get_underlying_type, unspecified_enum_class_refl);
static_assert(unspecified_enum_class_refl_underlying_type == ^int);

enum class specified_enum_class : int { };

constexpr meta::info specified_enum_class_refl = ^specified_enum_class;
static_assert(specified_enum_class_refl != ^int);

constexpr meta::info specified_enum_class_refl_underlying_type = __reflect(query_get_underlying_type, specified_enum_class_refl);
static_assert(specified_enum_class_refl_underlying_type == ^int);
