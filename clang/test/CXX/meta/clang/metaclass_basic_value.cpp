// RUN: %clang_cc1 -freflection -verify -std=c++2a %s

#include "reflection_iterator.h"
#include "reflection_mod.h"

using namespace meta;

consteval bool is_data_member(info x) {
  return __reflect(query_is_nonstatic_data_member, x);
}

consteval bool is_member_function(info x) {
  return __reflect(query_is_nonstatic_member_function, x);
}

consteval void make_public(info& x) {
  __reflect_mod(query_set_access, x, AccessModifier::Public);
}

consteval void make_private(info& x) {
  __reflect_mod(query_set_access, x, AccessModifier::Private);
}

consteval bool is_public(info x) {
  return __reflect(query_is_public, x);
}

consteval bool is_protected(info x) {
  return __reflect(query_is_protected, x);
}

consteval bool is_virtual(info x) {
  return __reflect(query_is_virtual, x);
}

consteval bool is_copy_assignment_operator(info x) {
  return __reflect(query_is_copy_assignment_operator, x);
}

consteval bool is_move_assignment_operator(info x) {
  return __reflect(query_is_move_assignment_operator, x);
}

consteval bool is_default_constructor(info x) {
  return __reflect(query_is_default_constructor, x);
}

consteval bool is_copy_constructor(info x) {
  return __reflect(query_is_copy_constructor, x);
}

consteval bool is_move_constructor(info x) {
  return __reflect(query_is_move_constructor, x);
}

consteval bool is_destructor(info x) {
  return __reflect(query_is_destructor, x);
}

consteval void compiler_error(const char* message) {
  __compiler_error(message);
}

consteval void compiler_require(bool condition, const char* message) {
  if (!condition)
    compiler_error(message);
}

consteval info definition_of(info type_reflection) {
  return __reflect(query_get_definition, type_reflection);
}

consteval void compiler_print_type_definition(info type_reflection) {
  (void) __reflect_pretty_print(definition_of(type_reflection));
}


//====================================================================
// Library code: implementing the metaclass (once)

consteval void BasicValue(info source) {
  bool needs_default_ctor = true;
  bool needs_copy_ctor = true;
  bool needs_move_ctor = true;
  bool needs_copy_assign_op = true;
  bool needs_move_assign_op = true;

  for (auto f : meta::range(definition_of(source))) {
    if (is_default_constructor(f))
      needs_default_ctor = false;
    if (is_copy_constructor(f))
      needs_copy_ctor = false;
    if (is_move_constructor(f))
      needs_move_ctor = false;
    if (is_copy_assignment_operator(f))
      needs_copy_assign_op = false;
    if (is_move_assignment_operator(f))
      needs_move_assign_op = false;
  }

  if (needs_default_ctor)
    -> fragment struct BasicValueDefaults {
      BasicValueDefaults() = default;
    };

  if (needs_copy_ctor)
    -> fragment struct BasicValueDefaults {
      BasicValueDefaults(const BasicValueDefaults& that) = default;
    };

  if (needs_move_ctor)
    -> fragment struct BasicValueDefaults {
      BasicValueDefaults(BasicValueDefaults&& that) = default;
    };

  if (needs_copy_assign_op)
    -> fragment struct BasicValueDefaults {
      BasicValueDefaults& operator=(const BasicValueDefaults& that) = default;
    };

  if (needs_move_assign_op)
    -> fragment struct BasicValueDefaults {
      BasicValueDefaults& operator=(BasicValueDefaults&& that) = default;
    };

  for (auto f : meta::range(definition_of(source))) {
    if (is_data_member(f)) {
      make_private(f);
    } else if (is_member_function(f)) {
      make_public(f);
      compiler_require(!is_protected(f), "a value type may not have a protected function");
      compiler_require(!is_virtual(f), "a value type may not have a virtual function");
      compiler_require(!is_destructor(f) || is_public(f), "a value destructor must be public");
    }
    -> f;
  }
};


//====================================================================
// User code: using the metaclass to write a type (many times)

class(BasicValue) Point {
  int x = 0, y = 0; // expected-note {{implicitly declared private here}}
  Point(int xx, int yy) : x{xx}, y{yy} { }
  Point() : Point(1, 1) { }
};

Point get_some_point() { return {1,1}; }

int main() {
  Point p1(50,100), p2;
  p2 = get_some_point();
  p2.x = 42; // expected-error {{'x' is a private member of 'Point'}}
}

consteval {
  compiler_print_type_definition(reflexpr(Point));
}
