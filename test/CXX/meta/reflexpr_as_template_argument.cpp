// RUN: %clang_cc1 -std=c++1z -freflection %s

template<auto T, auto V>
constexpr typename(T) test() {
  typename(T) x = valueof(V);
  return x;
}

struct S {
  int i;

  constexpr S(int i) : i(i) { }
};

int main() {
  constexpr int i = test<reflexpr(int), reflexpr(4)>();
  constexpr S s = test<reflexpr(S), reflexpr(S(2))>();

  static_assert(i == 4);
  static_assert(s.i == 2);

  return 0;
}
