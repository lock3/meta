// RUN: %clang_cc1 -fsyntax-only -verify -std=c++2a -freflection %s

int capture = 10;

class Foo {
  static constexpr auto test_cls_frag = fragment class {
    int i = 0;
    int i_capture = capture;

    void test_function() {
    }
  };

  static constexpr auto test_cls_self_frag = fragment class Frag {
    int i = 0;
    int i_capture = capture;

    void test_function(Frag &frag) {
    }
  };

  static constexpr auto test_cls_empty_frag = fragment class {
  };

  static constexpr auto test_cls_frag_without_body = fragment class; // expected-error {{expected class-fragment}}

  static constexpr auto test_struct_frag = fragment struct {
    int i = 0;
    int i_capture = capture;

    void test_function() {
    }
  };

  static constexpr auto test_struct_self_frag = fragment struct Frag {
    int i = 0;
    int i_capture = capture;

    void test_function(Frag& frag) {
    }
  };

  static constexpr auto test_struct_empty_frag = fragment struct {
  };

  static constexpr auto test_struct_frag_without_body = fragment struct; // expected-error {{expected class-fragment}}

  static constexpr auto test_union_frag = fragment union {
    int i_capture = capture;

    void test_function() {
    }
  };

  static constexpr auto test_union_self_frag = fragment union Frag {
    int i_capture = capture;

    void test_function(Frag& frag) {
    }
  };

  static constexpr auto test_union_empty_frag = fragment union {
  };

  static constexpr auto test_union_frag_without_body = fragment union; // expected-error {{expected class-fragment}}

  static constexpr auto test_ns_frag = fragment namespace {
    int i_capture = capture;

    void test_function() {
    }
  };

  static constexpr auto test_ns_self_frag = fragment namespace Frag {
    int i_capture = capture;

    using x = int;
    void test_function(Frag::x& x) {
    }
  };

  static constexpr auto test_ns_empty_frag = fragment union {
  };

  static constexpr auto test_ns_frag_without_body = fragment namespace; // expected-error {{expected namespace-fragment}}

  static constexpr auto unfinished_fragment = fragment; // expected-error {{expected fragment}}

  template<auto &x_val>
  class SomeClass {
    static constexpr auto cls_var = x_val;
  };

  static constexpr auto test_cls_frag_wrapper = SomeClass<test_cls_frag>();
  static constexpr auto test_cls_self_frag_wrapper = SomeClass<test_cls_self_frag>();
  static constexpr auto test_cls_empty_frag_wrapper = SomeClass<test_cls_empty_frag>();
  static constexpr auto test_struct_frag_wrapper = SomeClass<test_struct_frag>();
  static constexpr auto test_struct_self_frag_wrapper = SomeClass<test_struct_self_frag>();
  static constexpr auto test_struct_empty_frag_wrapper = SomeClass<test_struct_empty_frag>();
  static constexpr auto test_union_frag_wrapper = SomeClass<test_union_frag>();
  static constexpr auto test_union_self_frag_wrapper = SomeClass<test_union_self_frag>();
  static constexpr auto test_union_empty_frag_wrapper = SomeClass<test_union_empty_frag>();
  static constexpr auto test_ns_frag_wrapper = SomeClass<test_ns_frag>();
  static constexpr auto test_ns_self_frag_wrapper = SomeClass<test_ns_self_frag>();
  static constexpr auto test_ns_empty_frag_wrapper = SomeClass<test_ns_frag>();
};

int main() {
  return 0;
};
