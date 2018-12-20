// RUN: %clang_cc1 -freflection -std=c++1z %s

#include "reflection_query.h"
#include "reflection_traits.h"
#include "reflection_mod.h"
#include "reflection_iterator.h"

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

consteval bool is_member_function(meta::info refl) {
  return __reflect(query_is_nonstatic_member_function, refl)
      || __reflect(query_is_static_member_function, refl);
}

consteval bool is_copy(meta::info refl) {
  method_traits method(__reflect(query_get_decl_traits, refl));
  return method.is_copy_ctor || method.is_copy_assign;
}

consteval bool is_move(meta::info refl) {
  method_traits method(__reflect(query_get_decl_traits, refl));
  return method.is_move_ctor || method.is_move_assign;
}

consteval bool has_default_access(meta::info refl) {
  return __reflect(query_has_default_access, refl);
}

consteval bool is_public(meta::info refl) {
  access_traits access(__reflect(query_get_access_traits, refl));
  return access_traits(access).kind == public_access;
}

consteval void make_public(meta::info &refl) {
  __reflect_mod(query_set_access, refl, AccessModifier::Public);
}

consteval void make_pure_virtual(meta::info &refl) {
  __reflect_mod(query_set_add_pure_virtual, refl, true);
}

consteval int count_data_members(meta::info refl) {
  int total = 0;

  for (meta::info member : member_range(refl)) {
    if (is_data_member(member))
      ++total;
  }

  return total;
}

consteval void print_declaration(meta::info class_reflection) {
  auto class_decl_reflection = __reflect(query_get_parent, __reflect(query_get_begin, class_reflection));
  (void) __reflect_pretty_print(class_decl_reflection);
}

consteval void print_lines(int count) {
  for (int i = 0; i < count; ++i)
    (void) __reflect_print("");
}

//====================================================================
// Library code: implementing the metaclass (once)

consteval void interface(meta::info source) {
  compiler_require(count_data_members(source) == 0,
                   "interfaces may not contain data");

  for (meta::info f : member_range(source)) {
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
  print_declaration(reflexpr(Shape));
  print_lines(1);
  print_declaration(reflexpr(X));
}
