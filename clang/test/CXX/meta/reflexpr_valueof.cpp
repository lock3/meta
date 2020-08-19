// RUN: %clang_cc1 -std=c++2a -freflection %s

namespace meta {
  using info = decltype(reflexpr(void));
}

const int global1 = 75;
constexpr int global2 = 76;

enum E { A, B, C };

struct S {
  static constexpr int value = 4;
  int num = 12;
};

constexpr S s1;

int f() { return 0; }

template<meta::info refl, auto val>
void check_valueof_dependent() {
  static_assert(valueof(refl) == val);
}

int main() {
  static_assert(valueof(reflexpr(42)) == 42);
  check_valueof_dependent<reflexpr(42), 42>();

  static_assert(valueof(reflexpr(global1)) == 75);
  check_valueof_dependent<reflexpr(global1), 75>();

  static_assert(valueof(reflexpr(global2)) == 76);
  check_valueof_dependent<reflexpr(global2), 76>();

  static_assert(valueof(reflexpr(A)) == 0);
  check_valueof_dependent<reflexpr(A), 0>();

  static_assert(valueof(reflexpr(S::value)) == 4);
  check_valueof_dependent<reflexpr(S::value), 4>();

  static_assert(valueof(reflexpr(s1.num)) == 12);
  check_valueof_dependent<reflexpr(s1.num), 12>();

  static_assert(valueof(reflexpr(f)) == &f);
  check_valueof_dependent<reflexpr(f), &f>();

  static_assert(valueof(reflexpr(S::num)) == &S::num);
  check_valueof_dependent<reflexpr(S::num), &S::num>();
}
