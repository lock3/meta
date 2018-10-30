// RUN: %clang_cc1 -std=c++1z -freflection -verify %s

namespace bar {
  namespace fin {
  }
}

template<typename T>
void test_template() {
  constexpr auto int_reflexpr = reflexpr(int);
  T unqualid("foo_", reflexpr(bar), "_", reflexpr(bar::fin)) = T();

  int int_x = foo_bar_fin.unqualid("get_", unqualid("int_reflexpr"))();
  int int_y = unqualid("foo_bar_fin").get_int();
  int int_z = unqualid("foo_bar_fin").unqualid("get_int")();
}

template<typename T>
struct TemplateS {
  T unqualid("get_", reflexpr(T))() { return T(); }
};

template<typename T>
void test_template_class_attribute() {
  TemplateS<T> s;
  T res = s.unqualid("get_", reflexpr(T))();
}

struct S {
  int get_int() { return 0; }
};

void test_non_template() {
  constexpr auto int_reflexpr = reflexpr(int);
  S unqualid("foo_", reflexpr(bar), "_", reflexpr(bar::fin)) = S();

  int int_x = foo_bar_fin.unqualid("get_", unqualid("int_reflexpr"))();
  int int_y = unqualid("foo_bar_fin").get_int();
  int int_z = unqualid("foo_bar_fin").unqualid("get_int")();
}

void test_bad() {
  auto not_a_reflexpr = 1;

  S foo_bar_fin  = S();
  int int_x = foo_bar_fin.unqualid("get_nothing")(); // expected-error {{no member named 'get_nothing' in 'S'}}
  int int_y = foo_bar_fin.unqualid("get_", unqualid("not_a_reflexpr"))(); // expected-error {{expression is not an integral constant expression}}
}

int main() {
  test_template<int>();
  test_template<S>();
  test_template_class_attribute<int>();
  test_template_class_attribute<S>();
  test_non_template();
  test_bad();
  return 0;
}
