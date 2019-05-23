// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

int x = 0;

constexpr auto var_refl = reflexpr(x);
static_assert(!__reflect(query_is_type, var_refl));
static_assert(!__reflect(query_is_fundamental_type, var_refl));
static_assert(!__reflect(query_is_arithmetic_type, var_refl));
static_assert(!__reflect(query_is_scalar_type, var_refl));
static_assert(!__reflect(query_is_object_type, var_refl));
static_assert(!__reflect(query_is_compound_type, var_refl));

constexpr auto var_type_refl = __reflect(query_get_type, var_refl);
static_assert(__reflect(query_is_type, var_type_refl));
static_assert(__reflect(query_is_fundamental_type, var_type_refl));
static_assert(__reflect(query_is_arithmetic_type, var_type_refl));
static_assert(__reflect(query_is_scalar_type, var_type_refl));
static_assert(__reflect(query_is_object_type, var_type_refl));
static_assert(!__reflect(query_is_compound_type, var_type_refl));

void foo();

constexpr auto func_refl = reflexpr(foo);
static_assert(!__reflect(query_is_type, func_refl));
static_assert(!__reflect(query_is_fundamental_type, func_refl));
static_assert(!__reflect(query_is_arithmetic_type, func_refl));
static_assert(!__reflect(query_is_scalar_type, func_refl));
static_assert(!__reflect(query_is_object_type, func_refl));
static_assert(!__reflect(query_is_compound_type, func_refl));
static_assert(!__reflect(query_is_function_type, func_refl));

constexpr auto func_type_refl = __reflect(query_get_type, func_refl);
static_assert(__reflect(query_is_type, func_type_refl));
static_assert(!__reflect(query_is_fundamental_type, func_type_refl));
static_assert(!__reflect(query_is_arithmetic_type, func_type_refl));
static_assert(!__reflect(query_is_scalar_type, func_type_refl));
static_assert(!__reflect(query_is_object_type, func_type_refl));
static_assert(__reflect(query_is_compound_type, func_type_refl));
static_assert(__reflect(query_is_function_type, func_type_refl));

class class_decl;

constexpr auto class_refl = reflexpr(class_decl);
static_assert(__reflect(query_is_type, class_refl));
static_assert(!__reflect(query_is_fundamental_type, class_refl));
static_assert(!__reflect(query_is_arithmetic_type, class_refl));
static_assert(!__reflect(query_is_scalar_type, class_refl));
static_assert(__reflect(query_is_object_type, class_refl));
static_assert(__reflect(query_is_compound_type, class_refl));
static_assert(__reflect(query_is_class_type, class_refl));
static_assert(!__reflect(query_is_closure_type, class_refl));

union union_decl;

constexpr auto union_refl = reflexpr(union_decl);
static_assert(__reflect(query_is_type, union_refl));
static_assert(!__reflect(query_is_fundamental_type, union_refl));
static_assert(!__reflect(query_is_arithmetic_type, union_refl));
static_assert(!__reflect(query_is_scalar_type, union_refl));
static_assert(__reflect(query_is_object_type, union_refl));
static_assert(__reflect(query_is_compound_type, union_refl));
static_assert(__reflect(query_is_union_type, union_refl));

enum enum_decl : unsigned;

constexpr auto enum_refl = reflexpr(enum_decl);
static_assert(__reflect(query_is_type, enum_refl));
static_assert(!__reflect(query_is_fundamental_type, enum_refl));
static_assert(__reflect(query_is_arithmetic_type, enum_refl));
static_assert(__reflect(query_is_scalar_type, enum_refl));
static_assert(__reflect(query_is_object_type, enum_refl));
static_assert(__reflect(query_is_compound_type, enum_refl));
static_assert(__reflect(query_is_unscoped_enum, enum_refl));

enum class enum_class_decl;

constexpr auto enum_class_refl = reflexpr(enum_class_decl);
static_assert(__reflect(query_is_type, enum_class_refl));
static_assert(!__reflect(query_is_fundamental_type, enum_class_refl));
static_assert(!__reflect(query_is_arithmetic_type, enum_class_refl));
static_assert(__reflect(query_is_scalar_type, enum_class_refl));
static_assert(__reflect(query_is_object_type, enum_class_refl));
static_assert(__reflect(query_is_compound_type, enum_class_refl));
static_assert(__reflect(query_is_scoped_enum_type, enum_class_refl));

constexpr auto void_type_refl = reflexpr(void);
static_assert(__reflect(query_is_type, void_type_refl));
static_assert(__reflect(query_is_fundamental_type, void_type_refl));
static_assert(!__reflect(query_is_arithmetic_type, void_type_refl));
static_assert(!__reflect(query_is_scalar_type, void_type_refl));
static_assert(!__reflect(query_is_object_type, void_type_refl));
static_assert(!__reflect(query_is_compound_type, void_type_refl));
static_assert(__reflect(query_is_void_type, void_type_refl));

constexpr auto nullptr_expr_refl = reflexpr(nullptr);
static_assert(!__reflect(query_is_type, nullptr_expr_refl));
static_assert(!__reflect(query_is_fundamental_type, nullptr_expr_refl));
static_assert(!__reflect(query_is_arithmetic_type, nullptr_expr_refl));
static_assert(!__reflect(query_is_scalar_type, nullptr_expr_refl));
static_assert(!__reflect(query_is_object_type, nullptr_expr_refl));
static_assert(!__reflect(query_is_compound_type, nullptr_expr_refl));
static_assert(!__reflect(query_is_null_pointer_type, nullptr_expr_refl));

constexpr auto nullptr_type_refl = __reflect(query_get_type, nullptr_expr_refl);
static_assert(__reflect(query_is_type, nullptr_type_refl));
static_assert(!__reflect(query_is_fundamental_type, nullptr_type_refl));
static_assert(!__reflect(query_is_arithmetic_type, nullptr_type_refl));
static_assert(__reflect(query_is_scalar_type, nullptr_type_refl));
static_assert(__reflect(query_is_object_type, nullptr_type_refl));
static_assert(!__reflect(query_is_compound_type, nullptr_type_refl));
static_assert(__reflect(query_is_null_pointer_type, nullptr_type_refl));

constexpr auto char_type_refl = reflexpr(char);
static_assert(__reflect(query_is_type, char_type_refl));
static_assert(__reflect(query_is_fundamental_type, char_type_refl));
static_assert(__reflect(query_is_arithmetic_type, char_type_refl));
static_assert(__reflect(query_is_scalar_type, char_type_refl));
static_assert(__reflect(query_is_object_type, char_type_refl));
static_assert(!__reflect(query_is_compound_type, char_type_refl));
static_assert(__reflect(query_is_integral_type, char_type_refl));

constexpr auto unsigned_char_type_refl = reflexpr(unsigned char);
static_assert(__reflect(query_is_type, unsigned_char_type_refl));
static_assert(__reflect(query_is_fundamental_type, unsigned_char_type_refl));
static_assert(__reflect(query_is_arithmetic_type, unsigned_char_type_refl));
static_assert(__reflect(query_is_scalar_type, unsigned_char_type_refl));
static_assert(__reflect(query_is_object_type, unsigned_char_type_refl));
static_assert(!__reflect(query_is_compound_type, unsigned_char_type_refl));
static_assert(__reflect(query_is_integral_type, unsigned_char_type_refl));

constexpr auto short_type_refl = reflexpr(short);
static_assert(__reflect(query_is_type, short_type_refl));
static_assert(__reflect(query_is_fundamental_type, short_type_refl));
static_assert(__reflect(query_is_arithmetic_type, short_type_refl));
static_assert(__reflect(query_is_scalar_type, short_type_refl));
static_assert(__reflect(query_is_object_type, short_type_refl));
static_assert(!__reflect(query_is_compound_type, short_type_refl));
static_assert(__reflect(query_is_integral_type, short_type_refl));

constexpr auto unsigned_short_type_refl = reflexpr(unsigned short);
static_assert(__reflect(query_is_type, unsigned_short_type_refl));
static_assert(__reflect(query_is_fundamental_type, unsigned_short_type_refl));
static_assert(__reflect(query_is_arithmetic_type, unsigned_short_type_refl));
static_assert(__reflect(query_is_scalar_type, unsigned_short_type_refl));
static_assert(__reflect(query_is_object_type, unsigned_short_type_refl));
static_assert(!__reflect(query_is_compound_type, unsigned_short_type_refl));
static_assert(__reflect(query_is_integral_type, unsigned_short_type_refl));

constexpr auto int_type_refl = reflexpr(int);
static_assert(__reflect(query_is_type, int_type_refl));
static_assert(__reflect(query_is_fundamental_type, int_type_refl));
static_assert(__reflect(query_is_arithmetic_type, int_type_refl));
static_assert(__reflect(query_is_scalar_type, int_type_refl));
static_assert(__reflect(query_is_object_type, int_type_refl));
static_assert(!__reflect(query_is_compound_type, int_type_refl));
static_assert(__reflect(query_is_integral_type, int_type_refl));

constexpr auto unsigned_int_type_refl = reflexpr(unsigned int);
static_assert(__reflect(query_is_type, unsigned_int_type_refl));
static_assert(__reflect(query_is_fundamental_type, unsigned_int_type_refl));
static_assert(__reflect(query_is_arithmetic_type, unsigned_int_type_refl));
static_assert(__reflect(query_is_scalar_type, unsigned_int_type_refl));
static_assert(__reflect(query_is_object_type, unsigned_int_type_refl));
static_assert(!__reflect(query_is_compound_type, unsigned_int_type_refl));
static_assert(__reflect(query_is_integral_type, unsigned_int_type_refl));

constexpr auto long_type_refl = reflexpr(long);
static_assert(__reflect(query_is_type, long_type_refl));
static_assert(__reflect(query_is_fundamental_type, long_type_refl));
static_assert(__reflect(query_is_arithmetic_type, long_type_refl));
static_assert(__reflect(query_is_scalar_type, long_type_refl));
static_assert(__reflect(query_is_object_type, long_type_refl));
static_assert(!__reflect(query_is_compound_type, long_type_refl));
static_assert(__reflect(query_is_integral_type, long_type_refl));

constexpr auto unsigned_long_type_refl = reflexpr(unsigned long);
static_assert(__reflect(query_is_type, unsigned_long_type_refl));
static_assert(__reflect(query_is_fundamental_type, unsigned_long_type_refl));
static_assert(__reflect(query_is_arithmetic_type, unsigned_long_type_refl));
static_assert(__reflect(query_is_scalar_type, unsigned_long_type_refl));
static_assert(__reflect(query_is_object_type, unsigned_long_type_refl));
static_assert(!__reflect(query_is_compound_type, unsigned_long_type_refl));
static_assert(__reflect(query_is_integral_type, unsigned_long_type_refl));

constexpr auto long_long_type_refl = reflexpr(long long);
static_assert(__reflect(query_is_type, long_long_type_refl));
static_assert(__reflect(query_is_fundamental_type, long_long_type_refl));
static_assert(__reflect(query_is_arithmetic_type, long_long_type_refl));
static_assert(__reflect(query_is_scalar_type, long_long_type_refl));
static_assert(__reflect(query_is_object_type, long_long_type_refl));
static_assert(!__reflect(query_is_compound_type, long_long_type_refl));
static_assert(__reflect(query_is_integral_type, long_long_type_refl));

constexpr auto unsigned_long_long_type_refl = reflexpr(unsigned long long);
static_assert(__reflect(query_is_type, unsigned_long_long_type_refl));
static_assert(__reflect(query_is_fundamental_type, unsigned_long_long_type_refl));
static_assert(__reflect(query_is_arithmetic_type, unsigned_long_long_type_refl));
static_assert(__reflect(query_is_scalar_type, unsigned_long_long_type_refl));
static_assert(__reflect(query_is_object_type, unsigned_long_long_type_refl));
static_assert(!__reflect(query_is_compound_type, unsigned_long_long_type_refl));
static_assert(__reflect(query_is_integral_type, unsigned_long_long_type_refl));

constexpr auto float_type_refl = reflexpr(float);
static_assert(__reflect(query_is_type, float_type_refl));
static_assert(__reflect(query_is_fundamental_type, float_type_refl));
static_assert(__reflect(query_is_arithmetic_type, float_type_refl));
static_assert(__reflect(query_is_scalar_type, float_type_refl));
static_assert(__reflect(query_is_object_type, float_type_refl));
static_assert(!__reflect(query_is_compound_type, float_type_refl));
static_assert(__reflect(query_is_floating_point_type, float_type_refl));

constexpr auto double_type_refl = reflexpr(double);
static_assert(__reflect(query_is_type, double_type_refl));
static_assert(__reflect(query_is_fundamental_type, double_type_refl));
static_assert(__reflect(query_is_arithmetic_type, double_type_refl));
static_assert(__reflect(query_is_scalar_type, double_type_refl));
static_assert(__reflect(query_is_object_type, double_type_refl));
static_assert(!__reflect(query_is_compound_type, double_type_refl));
static_assert(__reflect(query_is_floating_point_type, double_type_refl));

constexpr auto long_double_type_refl = reflexpr(long double);
static_assert(__reflect(query_is_type, long_double_type_refl));
static_assert(__reflect(query_is_fundamental_type, long_double_type_refl));
static_assert(__reflect(query_is_arithmetic_type, long_double_type_refl));
static_assert(__reflect(query_is_scalar_type, long_double_type_refl));
static_assert(__reflect(query_is_object_type, long_double_type_refl));
static_assert(!__reflect(query_is_compound_type, long_double_type_refl));
static_assert(__reflect(query_is_floating_point_type, long_double_type_refl));

int arr[10];

constexpr auto array_refl = reflexpr(arr);
static_assert(!__reflect(query_is_type, array_refl));
static_assert(!__reflect(query_is_fundamental_type, array_refl));
static_assert(!__reflect(query_is_arithmetic_type, array_refl));
static_assert(!__reflect(query_is_scalar_type, array_refl));
static_assert(!__reflect(query_is_object_type, array_refl));
static_assert(!__reflect(query_is_compound_type, array_refl));
static_assert(!__reflect(query_is_array_type, array_refl));

constexpr auto array_type_refl = __reflect(query_get_type, array_refl);
static_assert(__reflect(query_is_type, array_type_refl));
static_assert(!__reflect(query_is_fundamental_type, array_type_refl));
static_assert(!__reflect(query_is_arithmetic_type, array_type_refl));
static_assert(!__reflect(query_is_scalar_type, array_type_refl));
static_assert(__reflect(query_is_object_type, array_type_refl));
static_assert(__reflect(query_is_compound_type, array_type_refl));
static_assert(__reflect(query_is_array_type, array_type_refl));

int *ptr;

constexpr auto ptr_refl = reflexpr(ptr);
static_assert(!__reflect(query_is_type, ptr_refl));
static_assert(!__reflect(query_is_fundamental_type, ptr_refl));
static_assert(!__reflect(query_is_arithmetic_type, ptr_refl));
static_assert(!__reflect(query_is_scalar_type, ptr_refl));
static_assert(!__reflect(query_is_object_type, ptr_refl));
static_assert(!__reflect(query_is_compound_type, ptr_refl));
static_assert(!__reflect(query_is_pointer_type, ptr_refl));

constexpr auto ptr_type_refl = __reflect(query_get_type, ptr_refl);
static_assert(__reflect(query_is_type, ptr_type_refl));
static_assert(!__reflect(query_is_fundamental_type, ptr_type_refl));
static_assert(!__reflect(query_is_arithmetic_type, ptr_type_refl));
static_assert(__reflect(query_is_scalar_type, ptr_type_refl));
static_assert(__reflect(query_is_object_type, ptr_type_refl));
static_assert(__reflect(query_is_compound_type, ptr_type_refl));
static_assert(__reflect(query_is_pointer_type, ptr_type_refl));

int &lvalue_var = *ptr;

constexpr auto lvalue_expr_refl = reflexpr(lvalue_var);
static_assert(!__reflect(query_is_type, lvalue_expr_refl));
static_assert(!__reflect(query_is_fundamental_type, lvalue_expr_refl));
static_assert(!__reflect(query_is_arithmetic_type, lvalue_expr_refl));
static_assert(!__reflect(query_is_scalar_type, lvalue_expr_refl));
static_assert(!__reflect(query_is_object_type, lvalue_expr_refl));
static_assert(!__reflect(query_is_compound_type, lvalue_expr_refl));
static_assert(!__reflect(query_is_lvalue_reference_type, lvalue_expr_refl));

constexpr auto lvalue_expr_type_refl = __reflect(query_get_type, lvalue_expr_refl);
static_assert(__reflect(query_is_type, lvalue_expr_type_refl));
static_assert(__reflect(query_is_fundamental_type, lvalue_expr_type_refl));
static_assert(__reflect(query_is_arithmetic_type, lvalue_expr_type_refl));
static_assert(__reflect(query_is_scalar_type, lvalue_expr_type_refl));
static_assert(__reflect(query_is_object_type, lvalue_expr_type_refl));
static_assert(!__reflect(query_is_compound_type, lvalue_expr_type_refl));
static_assert(!__reflect(query_is_lvalue_reference_type, lvalue_expr_type_refl));

constexpr auto lvalue_refl = __reflect(query_get_definition, lvalue_expr_refl);
static_assert(!__reflect(query_is_type, lvalue_refl));
static_assert(!__reflect(query_is_fundamental_type, lvalue_refl));
static_assert(!__reflect(query_is_arithmetic_type, lvalue_refl));
static_assert(!__reflect(query_is_scalar_type, lvalue_refl));
static_assert(!__reflect(query_is_object_type, lvalue_refl));
static_assert(!__reflect(query_is_compound_type, lvalue_refl));
static_assert(!__reflect(query_is_lvalue_reference_type, lvalue_refl));

constexpr auto lvalue_type_refl = __reflect(query_get_type, lvalue_refl);
static_assert(__reflect(query_is_type, lvalue_type_refl));
static_assert(!__reflect(query_is_fundamental_type, lvalue_type_refl));
static_assert(!__reflect(query_is_arithmetic_type, lvalue_type_refl));
static_assert(!__reflect(query_is_scalar_type, lvalue_type_refl));
static_assert(!__reflect(query_is_object_type, lvalue_type_refl));
static_assert(__reflect(query_is_compound_type, lvalue_type_refl));
static_assert(__reflect(query_is_lvalue_reference_type, lvalue_type_refl));

int &&rvalue_var = 2;

constexpr auto rvalue_expr_refl = reflexpr(rvalue_var);
static_assert(!__reflect(query_is_type, rvalue_expr_refl));
static_assert(!__reflect(query_is_fundamental_type, rvalue_expr_refl));
static_assert(!__reflect(query_is_arithmetic_type, rvalue_expr_refl));
static_assert(!__reflect(query_is_scalar_type, rvalue_expr_refl));
static_assert(!__reflect(query_is_object_type, rvalue_expr_refl));
static_assert(!__reflect(query_is_compound_type, rvalue_expr_refl));
static_assert(!__reflect(query_is_rvalue_reference_type, rvalue_expr_refl));

constexpr auto rvalue_expr_type_refl = __reflect(query_get_type, rvalue_expr_refl);
static_assert(__reflect(query_is_type, rvalue_expr_type_refl));
static_assert(__reflect(query_is_fundamental_type, rvalue_expr_type_refl));
static_assert(__reflect(query_is_arithmetic_type, rvalue_expr_type_refl));
static_assert(__reflect(query_is_scalar_type, rvalue_expr_type_refl));
static_assert(__reflect(query_is_object_type, rvalue_expr_type_refl));
static_assert(!__reflect(query_is_compound_type, rvalue_expr_type_refl));
static_assert(!__reflect(query_is_rvalue_reference_type, rvalue_expr_type_refl));

constexpr auto rvalue_refl = __reflect(query_get_definition, rvalue_expr_refl);
static_assert(!__reflect(query_is_type, rvalue_refl));
static_assert(!__reflect(query_is_fundamental_type, rvalue_refl));
static_assert(!__reflect(query_is_arithmetic_type, rvalue_refl));
static_assert(!__reflect(query_is_scalar_type, rvalue_refl));
static_assert(!__reflect(query_is_object_type, rvalue_refl));
static_assert(!__reflect(query_is_compound_type, rvalue_refl));
static_assert(!__reflect(query_is_rvalue_reference_type, rvalue_refl));

constexpr auto rvalue_type_refl = __reflect(query_get_type, rvalue_refl);
static_assert(__reflect(query_is_type, rvalue_type_refl));
static_assert(!__reflect(query_is_fundamental_type, rvalue_type_refl));
static_assert(!__reflect(query_is_arithmetic_type, rvalue_type_refl));
static_assert(!__reflect(query_is_scalar_type, rvalue_type_refl));
static_assert(!__reflect(query_is_object_type, rvalue_type_refl));
static_assert(__reflect(query_is_compound_type, rvalue_type_refl));
static_assert(__reflect(query_is_rvalue_reference_type, rvalue_type_refl));

struct Container {
  int memb_var;

  int memb_fn();
};

constexpr auto member_obj_pointer_refl = reflexpr(&Container::memb_var);
static_assert(!__reflect(query_is_type, member_obj_pointer_refl));
static_assert(!__reflect(query_is_fundamental_type, member_obj_pointer_refl));
static_assert(!__reflect(query_is_arithmetic_type, member_obj_pointer_refl));
static_assert(!__reflect(query_is_scalar_type, member_obj_pointer_refl));
static_assert(!__reflect(query_is_object_type, member_obj_pointer_refl));
static_assert(!__reflect(query_is_compound_type, member_obj_pointer_refl));
static_assert(!__reflect(query_is_member_object_pointer_type, member_obj_pointer_refl));

constexpr auto member_obj_pointer_type_refl = __reflect(query_get_type, member_obj_pointer_refl);
static_assert(__reflect(query_is_type, member_obj_pointer_type_refl));
static_assert(!__reflect(query_is_fundamental_type, member_obj_pointer_type_refl));
static_assert(!__reflect(query_is_arithmetic_type, member_obj_pointer_type_refl));
static_assert(__reflect(query_is_scalar_type, member_obj_pointer_type_refl));
static_assert(__reflect(query_is_object_type, member_obj_pointer_type_refl));
static_assert(__reflect(query_is_compound_type, member_obj_pointer_type_refl));
static_assert(__reflect(query_is_member_object_pointer_type, member_obj_pointer_type_refl));

constexpr auto member_fn_pointer_refl = reflexpr(&Container::memb_fn);
static_assert(!__reflect(query_is_type, member_fn_pointer_refl));
static_assert(!__reflect(query_is_fundamental_type, member_fn_pointer_refl));
static_assert(!__reflect(query_is_arithmetic_type, member_fn_pointer_refl));
static_assert(!__reflect(query_is_scalar_type, member_fn_pointer_refl));
static_assert(!__reflect(query_is_object_type, member_fn_pointer_refl));
static_assert(!__reflect(query_is_compound_type, member_fn_pointer_refl));
static_assert(!__reflect(query_is_member_function_pointer_type, member_fn_pointer_refl));

constexpr auto member_fn_pointer_type_refl = __reflect(query_get_type, member_fn_pointer_refl);
static_assert(__reflect(query_is_type, member_fn_pointer_type_refl));
static_assert(!__reflect(query_is_fundamental_type, member_fn_pointer_type_refl));
static_assert(!__reflect(query_is_arithmetic_type, member_fn_pointer_type_refl));
static_assert(__reflect(query_is_scalar_type, member_fn_pointer_type_refl));
static_assert(__reflect(query_is_object_type, member_fn_pointer_type_refl));
static_assert(__reflect(query_is_compound_type, member_fn_pointer_type_refl));
static_assert(__reflect(query_is_member_function_pointer_type, member_fn_pointer_type_refl));

auto closure = []{ return 0; };

constexpr auto closure_refl = reflexpr(closure);
static_assert(!__reflect(query_is_type, closure_refl));
static_assert(!__reflect(query_is_fundamental_type, closure_refl));
static_assert(!__reflect(query_is_arithmetic_type, closure_refl));
static_assert(!__reflect(query_is_scalar_type, closure_refl));
static_assert(!__reflect(query_is_object_type, closure_refl));
static_assert(!__reflect(query_is_compound_type, closure_refl));
static_assert(!__reflect(query_is_closure_type, closure_refl));

constexpr auto closure_type_refl = __reflect(query_get_type, closure_refl);
static_assert(__reflect(query_is_type, closure_type_refl));
static_assert(!__reflect(query_is_fundamental_type, closure_type_refl));
static_assert(!__reflect(query_is_arithmetic_type, closure_type_refl));
static_assert(!__reflect(query_is_scalar_type, closure_type_refl));
static_assert(__reflect(query_is_object_type, closure_type_refl));
static_assert(__reflect(query_is_compound_type, closure_type_refl));
static_assert(__reflect(query_is_closure_type, closure_type_refl));
