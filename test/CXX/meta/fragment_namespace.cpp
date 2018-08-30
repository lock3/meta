// RUN: %clang_cc1 -I%S/usr/include -I%S/usr/local/include/c++/v1 -fsyntax-only -verify -std=c++1z -freflection %s

#include <experimental/meta>

namespace foo {

constexpr auto test_cls_frag = __fragment class {
  int i = 0;

  void test_function() {
  }
};

constexpr auto test_cls_self_frag = __fragment class Frag {
  int i = 0;

  void test_function(Frag& frag) {
  }
};

constexpr auto test_struct_frag = __fragment struct {
  int i = 0;

  void test_function() {
  }
};

constexpr auto test_struct_self_frag = __fragment struct Frag {
  int i = 0;

  void test_function(Frag& frag) {
  }
};

constexpr auto test_union_frag = __fragment union {
  int i = 0;

  void test_function() {
  }
};

constexpr auto test_union_self_frag = __fragment union Frag {
  int i = 0;

  void test_function(Frag& frag) {
  }
};

constexpr auto test_ns_frag = __fragment namespace {
  void test_function() {
  }
};

// constexpr auto test_ns_self_frag = __fragment namespace Frag {
//   void test_function(Frag& frag) {
//   }
// };

constexpr auto unfinished_fragment = __fragment; // expected-error {{expected fragment}}

template<auto &x_val>
class SomeClass {
  static constexpr auto cls_var = x_val;
};

constexpr auto test_cls_frag_wrapper = SomeClass<test_cls_frag>();
constexpr auto test_cls_self_frag_wrapper = SomeClass<test_cls_self_frag>();
constexpr auto test_struct_frag_wrapper = SomeClass<test_struct_frag>();
constexpr auto test_struct_self_frag_wrapper = SomeClass<test_struct_self_frag>();
constexpr auto test_union_frag_wrapper = SomeClass<test_union_frag>();
constexpr auto test_union_self_frag_wrapper = SomeClass<test_union_self_frag>();
constexpr auto test_ns_frag_wrapper = SomeClass<test_ns_frag>();
// constexpr auto test_ns_self_frag_wrapper = SomeClass<test_ns_self_frag>();

constexpr {
};

};

int main() {
  return 0;
};

