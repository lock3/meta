// RUN: %clang_cc1 -fsyntax-only -verify -std=c++2a -freflection -Wno-deprecated-fragment %s

int capture = 10;

template<auto &x_val>
class SomeClass {
  static constexpr auto cls_var = x_val;
};

int main() {
  constexpr auto test_cls_frag = __fragment class {
    int i = 0;
    int i_capture = capture;

    void test_function() {
    }
  };

  constexpr auto test_cls_self_frag = __fragment class Frag {
    int i = 0;
    int i_capture = capture;

    void test_function(Frag& frag) {
    }
  };

  constexpr auto test_cls_empty_frag = __fragment class {
  };

  constexpr auto test_cls_frag_without_body = __fragment class; // expected-error {{expected class-fragment}}

  constexpr auto test_struct_frag = __fragment struct {
    int i = 0;
    int i_capture = capture;

    void test_function() {
    }
  };

  constexpr auto test_struct_self_frag = __fragment struct Frag {
    int i = 0;
    int i_capture = capture;

    void test_function(Frag& frag) {
    }
  };

  constexpr auto test_struct_empty_frag = __fragment struct {
  };

  constexpr auto test_struct_frag_without_body = __fragment struct; // expected-error {{expected class-fragment}}

  constexpr auto test_union_frag = __fragment union {
    int i_capture = capture;

    void test_function() {
    }
  };

  constexpr auto test_union_self_frag = __fragment union Frag {
    int i_capture = capture;

    void test_function(Frag& frag) {
    }
  };

  constexpr auto test_union_empty_frag = __fragment union {
  };

  constexpr auto test_union_frag_without_body = __fragment union; // expected-error {{expected class-fragment}}

  constexpr auto test_ns_frag = __fragment namespace {
    int i_capture = capture;

    void test_function() {
    }
  };

  constexpr auto test_ns_self_frag = __fragment namespace Frag {
    int i_capture = capture;

    using x = int;
    void test_function(Frag::x& x) {
    }
  };

  constexpr auto test_ns_empty_frag = __fragment namespace {
  };

  constexpr auto test_block_frag = __fragment {
    int i = 0;
    int i_capture = capture;
  };

  constexpr auto test_empty_block_frag = __fragment {
  };

  constexpr auto test_ns_frag_without_body = __fragment namespace; // expected-error {{expected namespace-fragment}}

  constexpr auto unfinished_fragment = __fragment; // expected-error {{expected fragment}}

  // constexpr auto test_cls_frag_wrapper = SomeClass<test_cls_frag>();
  // constexpr auto test_cls_self_frag_wrapper = SomeClass<test_cls_self_frag>();
  // constexpr auto test_cls_empty_frag_wrapper = SomeClass<test_cls_empty_frag>();
  // constexpr auto test_struct_frag_wrapper = SomeClass<test_struct_frag>();
  // constexpr auto test_struct_self_frag_wrapper = SomeClass<test_struct_self_frag>();
  // constexpr auto test_struct_empty_frag_wrapper = SomeClass<test_struct_empty_frag>();
  // constexpr auto test_union_frag_wrapper = SomeClass<test_union_frag>();
  // constexpr auto test_union_self_frag_wrapper = SomeClass<test_union_self_frag>();
  // constexpr auto test_union_empty_frag_wrapper = SomeClass<test_union_empty_frag>();
  // constexpr auto test_ns_frag_wrapper = SomeClass<test_ns_frag>();
  // constexpr auto test_ns_self_frag_wrapper = SomeClass<test_ns_self_frag>();
  // constexpr auto test_ns_empty_frag_wrapper = SomeClass<test_ns_empty_frag>();

  return 0;
};
