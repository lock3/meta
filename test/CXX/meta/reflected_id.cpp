// RUN: %clang_cc1 -std=c++1z -freflection %s

namespace bar {
  namespace fin { }
}

template<typename T>
T test_template() {
  constexpr auto int_reflexpr = reflexpr(int);
  T (. "foo_", reflexpr(bar), "_", reflexpr(bar::fin) .) = T();

  int int_x = foo_bar_fin.(. "get_", (. "int_reflexpr" .) .)();
  int int_y = (. "foo_bar_fin" .).get_int();
  int int_z = (. "foo_bar_fin" .).(. "get_int" .)();

  return foo_bar_fin;
}

struct S {
  int get_int() { return 0; }
};

S test_non_template() {
  constexpr auto int_reflexpr = reflexpr(int);
  S (. "foo_", reflexpr(bar), "_", reflexpr(bar::fin) .) = S();

  int int_x = foo_bar_fin.(. "get_", (. "int_reflexpr" .) .)();
  int int_y = (. "foo_bar_fin" .).get_int();
  int int_z = (. "foo_bar_fin" .).(. "get_int" .)();

  return foo_bar_fin;
}

int main() {
  S s_x = test_template<S>();
  S s_y = test_non_template();
  return 0;
}
