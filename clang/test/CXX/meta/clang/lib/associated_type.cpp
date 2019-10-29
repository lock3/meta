// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

constexpr meta::info expr_refl = reflexpr(2 + 2);
static_assert(expr_refl != reflexpr(int));

constexpr meta::info expr_type_refl = __reflect(query_get_type, expr_refl);
static_assert(expr_type_refl == reflexpr(int));

namespace type_decl_container {
  using special_type = int;
}

constexpr meta::info type_decl_refl = __reflect(query_get_begin, reflexpr(type_decl_container));
static_assert(type_decl_refl != reflexpr(int));

constexpr meta::info type_decl_type_refl = __reflect(query_get_type, type_decl_refl);
static_assert(type_decl_type_refl == reflexpr(int));

namespace value_decl_container {
  int value_decl = 10;
}

constexpr meta::info value_decl_refl = __reflect(query_get_begin, reflexpr(value_decl_container));
static_assert(value_decl_refl != reflexpr(int));

constexpr meta::info value_decl_type_refl = __reflect(query_get_type, value_decl_refl);
static_assert(value_decl_type_refl == reflexpr(int));

int function() { return 0; }

constexpr meta::info function_decl_refl = reflexpr(function);
static_assert(function_decl_refl != reflexpr(int));

constexpr meta::info function_decl_return_type_refl = __reflect(query_get_return_type, function_decl_refl);
static_assert(function_decl_return_type_refl == reflexpr(int));

using function_type = int (*)(float);

constexpr meta::info function_pointer_type_refl = reflexpr(function_type);
static_assert(function_pointer_type_refl != reflexpr(int));

constexpr meta::info function_type_refl = __reflect(query_remove_pointer, function_pointer_type_refl);
static_assert(function_type_refl != reflexpr(int));

constexpr meta::info function_type_return_type_refl = __reflect(query_get_return_type, function_type_refl);
static_assert(function_type_return_type_refl == reflexpr(int));

struct clazz {
  void method() { }

  void const_method() const { }
};

constexpr meta::info method_refl = reflexpr(clazz::method);
static_assert(method_refl != reflexpr(clazz));

constexpr meta::info method_this_type_refl = __reflect(query_get_this_ref_type, method_refl);
static_assert(method_this_type_refl == reflexpr(clazz*));

constexpr meta::info const_method_refl = reflexpr(clazz::const_method);
static_assert(const_method_refl != reflexpr(clazz));

constexpr meta::info const_method_this_type_refl = __reflect(query_get_this_ref_type, const_method_refl);
static_assert(const_method_this_type_refl == reflexpr(const clazz*));

enum unspecified_enum { };

constexpr meta::info unspecified_enum_refl = reflexpr(unspecified_enum);
static_assert(unspecified_enum_refl != reflexpr(unsigned));

constexpr meta::info unspecified_enum_refl_underlying_type = __reflect(query_get_underlying_type, unspecified_enum_refl);
static_assert(unspecified_enum_refl_underlying_type == reflexpr(unsigned));

enum specified_enum : int { };

constexpr meta::info specified_enum_refl = reflexpr(specified_enum);
static_assert(specified_enum_refl != reflexpr(int));

constexpr meta::info specified_enum_refl_underlying_type = __reflect(query_get_underlying_type, specified_enum_refl);
static_assert(specified_enum_refl_underlying_type == reflexpr(int));

enum class unspecified_enum_class { };

constexpr meta::info unspecified_enum_class_refl = reflexpr(unspecified_enum_class);
static_assert(unspecified_enum_class_refl != reflexpr(int));

constexpr meta::info unspecified_enum_class_refl_underlying_type = __reflect(query_get_underlying_type, unspecified_enum_class_refl);
static_assert(unspecified_enum_class_refl_underlying_type == reflexpr(int));

enum class specified_enum_class : int { };

constexpr meta::info specified_enum_class_refl = reflexpr(specified_enum_class);
static_assert(specified_enum_class_refl != reflexpr(int));

constexpr meta::info specified_enum_class_refl_underlying_type = __reflect(query_get_underlying_type, specified_enum_class_refl);
static_assert(specified_enum_class_refl_underlying_type == reflexpr(int));
