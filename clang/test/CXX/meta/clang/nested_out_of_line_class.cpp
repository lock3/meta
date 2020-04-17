// RUN: %clang_cc1 -freflection -std=c++2a %s

namespace meta {
  using info = decltype(reflexpr(void));
}

consteval auto generate_func() {
  return fragment struct S {
    int int_val = 12;

    constexpr S() = default;

    consteval int get_int() const {
      return 24;
    }
  };
}

struct A {
  struct NestedA;
};

struct A::NestedA {
  consteval -> generate_func();
};

int main() {
  constexpr A::NestedA nested_class;
  static_assert(nested_class.int_val == 12);
  static_assert(nested_class.get_int() == 24);
  return 0;
}
