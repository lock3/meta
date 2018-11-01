// RUN: %clang_cc1 -std=c++1z -freflection %s

template<auto T>
typename(T) test() {
  typename(T) x;
  return x;
}

struct S { };

int main() {
  int i = test<reflexpr(int)>();
  S s = test<reflexpr(S)>();
  return 0;
}
