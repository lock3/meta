// RUN: %clang_cc1 -freflection -std=c++1z %s

#include "reflection_query.h"
#include "reflection_traits.h"
#include "reflection_mod.h"
#include "reflection_iterator.h"

using namespace meta;

//====================================================================
// Library code: assisting the metaclass implementation, and providing
//               methods otherwise undefined in the test suite.

consteval void compiler_require(bool condition, const char* error_msg) {
  if (!condition)
    __compiler_error(error_msg);
}

consteval bool is_data_member(meta::info refl) {
  return __reflect(query_is_nonstatic_data_member, refl)
      || __reflect(query_is_static_data_member, refl);
}

consteval bool is_member_function(info refl) {
  return __reflect(query_is_nonstatic_member_function, refl)
      || __reflect(query_is_static_member_function, refl);
}

consteval bool is_copy(info refl) {
  method_traits method(__reflect(query_get_decl_traits, refl));
  return method.is_copy_ctor || method.is_copy_assign;
}

consteval bool is_move(info refl) {
  method_traits method(__reflect(query_get_decl_traits, refl));
  return method.is_move_ctor || method.is_move_assign;
}

consteval bool has_default_access(info refl) {
  return __reflect(query_has_default_access, refl);
}

consteval bool is_public(info refl) {
  access_traits access(__reflect(query_get_access_traits, refl));
  return access_traits(access).kind == public_access;
}

consteval void make_public(info &refl) {
  __reflect_mod(query_set_access, refl, AccessModifier::Public);
}

consteval void make_pure_virtual(info &refl) {
  __reflect_mod(query_set_add_pure_virtual, refl, true);
}

consteval int count_data_members(info refl) {
  int total = 0;

  for (info member : member_range(refl)) {
    if (is_data_member(member))
      ++total;
  }

  return total;
}

consteval info definition_of(info type_reflection) {
  return __reflect(query_get_definition, type_reflection);
}

consteval void compiler_print_type_definition(info type_reflection) {
  (void) __reflect_pretty_print(definition_of(type_reflection));
}

consteval void compiler_print_lines(int count) {
  for (int i = 0; i < count; ++i)
    (void) __reflect_print("");
}

//====================================================================
// Library code: implementing the metaclass (once)

consteval void interface(info source) {
  compiler_require(count_data_members(source) == 0,
                   "interfaces may not contain data");

  for (info f : member_range(source)) {
    compiler_require(!is_copy(f) && !is_move(f),
       "interfaces may not copy or move; consider"
       " a virtual clone() instead");

    if (!has_default_access(f))
      make_public(f);

    compiler_require(is_public(f), "interface functions must be public");

    make_pure_virtual(f);

    -> f;
  }

  -> __fragment struct X { virtual ~X() noexcept {} };
};


//====================================================================
// User code: using the metaclass to write a type (many times)

struct(interface) Shape {
    int area() const;
    void scale_by(double factor);
};

class X : public Shape {
    int area() const { return 42; }
    void scale_by(double factor) { }
};

int main() {
  X x;
  return 0;
}

consteval {
  compiler_print_type_definition(reflexpr(Shape));
  compiler_print_lines(1);
  compiler_print_type_definition(reflexpr(X));
}
