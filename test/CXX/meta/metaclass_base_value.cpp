// RUN: %clang_cc1 -freflection -verify -std=c++1z %s

#include "reflection_iterator.h"
#include "reflection_traits.h"
#include "reflection_mod.h"

namespace meta {

// WIP: This is not right.
consteval info definition_of(meta::info class_reflection) {
  return __reflect(query_get_parent, __reflect(query_get_begin, class_reflection));
}

consteval void print_definition(meta::info class_reflection) {
  (void) __reflect_pretty_print(definition_of(class_reflection));
}

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
  return access_traits(__reflect(query_get_access_traits, x)).kind == access_kind::public_access;
}

consteval bool is_protected(info x) {
  return access_traits(__reflect(query_get_access_traits, x)).kind == access_kind::protected_access;
}

consteval bool is_virtual(info x) {
  return method_traits(__reflect(query_get_decl_traits, x)).is_virtual;
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

}

//====================================================================
// Library code: implementing the metaclass (once)

consteval void BasicValue(meta::info source) {
  -> __fragment struct BasicValueDefaults {
    BasicValueDefaults() = default;
    BasicValueDefaults(const BasicValueDefaults& that) = default;
    BasicValueDefaults(BasicValueDefaults&& that) = default;
    BasicValueDefaults& operator=(const BasicValueDefaults& that) = default;
    BasicValueDefaults& operator=(BasicValueDefaults&& that) = default;
  };

  for (auto f : member_range(meta::definition_of(source))) {
    if (meta::is_data_member(f)) {
      meta::make_private(f);
    } else if (meta::is_member_function(f)) {
      meta::make_public(f);
      meta::compiler_require(!meta::is_protected(f), "a value type may not have a protected function");
      meta::compiler_require(!meta::is_virtual(f), "a value type may not have a virtual function");
      meta::compiler_require(!meta::is_destructor(f) || meta::is_public(f), "a value destructor must be public");
    }
    -> f;
  }
};


//====================================================================
// User code: using the metaclass to write a type (many times)

class(BasicValue) Point {
  int x = 0, y = 0; // expected-note {{implicitly declared private here}}
  Point(int xx, int yy) : x{xx}, y{yy} { }
};

Point get_some_point() { return {1,1}; }

int main() {
  Point p1(50,100), p2;
  p2 = get_some_point();
  p2.x = 42; // expected-error {{'x' is a private member of 'Point'}}
}

consteval {
  meta::print_definition(reflexpr(Point));
}
