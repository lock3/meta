// RUN: %clang_cc1 -std=c++1z -freflection %s

template<typename T>
T test() {
  constexpr auto t = reflexpr(T);
  typename(t) tv = T();
  return tv;
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
