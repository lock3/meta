// RUN: %clang_cc1 -std=c++2a -freflection %s

namespace meta {
  using info = decltype(^void);
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
void check_value_expression_splice_dependent() {
  static_assert([:refl:] == val);
}

int main() {
  static_assert([: ^42 :] == 42);
  check_value_expression_splice_dependent<^42, 42>();

  static_assert([: ^global1 :] == 75);
  check_value_expression_splice_dependent<^global1, 75>();

  static_assert([: ^global2 :] == 76);
  check_value_expression_splice_dependent<^global2, 76>();

  static_assert([: ^A :] == 0);
  check_value_expression_splice_dependent<^A, 0>();

  static_assert([: ^S::value :] == 4);
  check_value_expression_splice_dependent<^S::value, 4>();

  static_assert([: ^s1.num :] == 12);
  check_value_expression_splice_dependent<^s1.num, 12>();

  static_assert([: ^f :] == &f);
  check_value_expression_splice_dependent<^f, &f>();
}
