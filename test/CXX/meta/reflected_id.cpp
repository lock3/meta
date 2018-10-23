// RUN: %clang_cc1 -std=c++1z -freflection -verify %s

namespace bar {
  namespace fin { }
}

template<typename T>
T test_template() {
  constexpr auto int_reflexpr = reflexpr(int);
  T unqualid("foo_", reflexpr(bar), "_", reflexpr(bar::fin)) = T();

  int int_x = foo_bar_fin.unqualid("get_", unqualid("int_reflexpr"))();
  int int_y = unqualid("foo_bar_fin").get_int();
  int int_z = unqualid("foo_bar_fin").unqualid("get_int")();

  return foo_bar_fin;
}

struct S {
  int get_int() { return 0; }
};

S test_non_template() {
  constexpr auto int_reflexpr = reflexpr(int);
  S unqualid("foo_", reflexpr(bar), "_", reflexpr(bar::fin)) = S();

  int int_x = foo_bar_fin.unqualid("get_", unqualid("int_reflexpr"))();
  int int_y = unqualid("foo_bar_fin").get_int();
  int int_z = unqualid("foo_bar_fin").unqualid("get_int")();

  return foo_bar_fin;
}

void test_bad() {
  auto not_a_reflexpr = 1;

  S foo_bar_fin  = S();
  int int_x = foo_bar_fin.unqualid("get_nothing")(); // expected-error {{no member named 'get_nothing' in 'S'}}
  int int_y = foo_bar_fin.unqualid("get_", unqualid("not_a_reflexpr"))(); // expected-error {{expression is not an integral constant expression}}
}

int main() {
  S s_x = test_template<S>();
  S s_y = test_non_template();
  test_bad();
  return 0;
}
