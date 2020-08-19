// RUN: %clang_cc1 -freflection -std=c++2a %s

#include "reflection_query.h"
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
  return __reflect(query_is_copy_constructor, refl)
      || __reflect(query_is_copy_assignment_operator, refl);
}

consteval bool is_move(info refl) {
  return __reflect(query_is_move_constructor, refl)
      || __reflect(query_is_move_assignment_operator, refl);
}

consteval bool has_default_access(info refl) {
  return __reflect(query_has_default_access, refl);
}

consteval bool is_public(info refl) {
  return __reflect(query_is_public, refl);
}

consteval void make_public(info &refl) {
  __reflect_mod(query_set_access, refl, AccessModifier::Public);
}

consteval void make_pure_virtual(info &refl) {
  __reflect_mod(query_set_add_pure_virtual, refl, true);
}

consteval int count_data_members(info refl) {
  int total = 0;

  for (info member : meta::range(refl)) {
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
  for (info mem : meta::range(source)) {
    compiler_require(!is_data_member(mem), "interfaces may not contain data");
    compiler_require(!is_copy(mem) && !is_move(mem),
       "interfaces may not copy or move; consider"
       " a virtual clone() instead");

    if (has_default_access(mem))
      make_public(mem);

    compiler_require(is_public(mem), "interface functions must be public");

    make_pure_virtual(mem);

    -> mem;
  }

  -> fragment struct X { virtual ~X() noexcept {} };
};


//====================================================================
// User code: using the metaclass to write a type (many times)

class(interface) Shape {
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
