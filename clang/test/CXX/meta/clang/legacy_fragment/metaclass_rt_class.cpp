// RUN: %clang_cc1 -freflection -Wno-deprecated-fragment -std=c++2a %s

#include "../reflection_query.h"
#include "../reflection_mod.h"
#include "../reflection_iterator.h"

using namespace meta;

using HRESULT = long;

// Placeholder for constexpr string.
using string_type = const char*;

consteval string_type name_of(info x) {
  return __reflect(query_get_name, x);
}

consteval info return_type_of(info x) {
  return __reflect(query_get_return_type, x);
}

consteval bool is_data_member(info refl) {
  return __reflect(query_is_nonstatic_data_member, refl)
      || __reflect(query_is_static_data_member, refl);
}

consteval bool is_member_function(info refl) {
  return __reflect(query_is_nonstatic_member_function, refl)
      || __reflect(query_is_static_member_function, refl);
}

consteval void make_public(info &refl) {
  __reflect_mod(query_set_access, refl, AccessModifier::Public);
}

consteval void make_private(info &refl) {
  __reflect_mod(query_set_access, refl, AccessModifier::Private);
}

consteval info definition_of(info type_reflection) {
  return __reflect(query_get_definition, type_reflection);
}

consteval void compiler_print_type_definition(info type_reflection) {
  (void) __reflect_pretty_print(definition_of(type_reflection));
}

consteval void gen_impl(info proto) {
  for (info mem : meta::range(proto)) {
    // Handle data members
    if (is_data_member(mem)) {
      make_private(mem);
      -> mem;
      continue;
    }

    // Handle member functions
    if (is_member_function(mem)) {
      make_public(mem);
      -> mem;
      continue;
    }
  }
}

consteval void gen_void_fn(info fn) {
  meta::range params(fn);
  -> __fragment struct {
    virtual HRESULT unqualid(name_of(fn))(-> params) {
      this->m_impl.unqualid(name_of(fn))(unqualid(... params));
      return HRESULT();
    }
  };
}

consteval void gen_non_void_fn(info fn) {
  info ret_type = return_type_of(fn);
  meta::range params(fn);
  -> __fragment struct {
    virtual HRESULT unqualid(name_of(fn))(-> params, typename(ret_type)* retval) {
      *retval = this->m_impl.unqualid(name_of(fn))(unqualid(... params));
      return HRESULT();
    }
  };
}

consteval void rt_class(info proto) {
  // Create the implementation class.
  -> __fragment class {
    class impl_type {
      consteval { gen_impl(proto); }
    };
  };

  // Generate a member of the type.
  -> __fragment class C {
    typename C::impl_type m_impl;
  };

  // Generate wrappers that call into the implementation.
  for (info mem : meta::range(proto)) {
    if (!is_member_function(mem))
      continue;

    auto ret_type = return_type_of(mem);
    if (ret_type == reflexpr(void))
      gen_void_fn(mem);
    else
      gen_non_void_fn(mem);
  }
}

class(rt_class) Circle {
  int data1, data2;
  int g(double d) { return (int)d; }
  void h() { return; }
  int f(int i, int j) { return i+j; }
};

consteval {
  compiler_print_type_definition(reflexpr(Circle));
}

int main() {
  Circle c;

  // c.h();

  // int ret;
  // c.f(&ret);
}
