// RUN: %clang_cc1 -std=c++2a -freflection %s

template<auto T, auto V>
constexpr typename [:T:] test() {
  typename [:T:] x = [:V:];
  return x;
}

struct S {
  int i;

  constexpr S(int i) : i(i) { }
};

int main() {
  constexpr int i = test<^int, ^4>();
  constexpr S s = test<^S, ^S(2)>();

  static_assert(i == 4);
  static_assert(s.i == 2);

  return 0;
}
