// RUN: %clang_cc1 -std=c++1z -freflection %s

namespace bar { }

template<typename T>
T test() {
  T (. "foo_", reflexpr(bar) .) = T();
  return foo_bar;
}

struct S { };

void test_template() {
  int int_val = test<int>();
  S s_val = test<S>();
}

int main() {
  test_template();
  return 0;
}
