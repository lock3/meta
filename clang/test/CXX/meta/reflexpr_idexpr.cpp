// RUN: %clang_cc1 -std=c++2a -freflection %s

namespace meta {
  using info = decltype(^void);
}

int global0 = 19;
const int global1 = 75;
constexpr int global2 = 76;

enum E { A, B, C };

struct S {
  static constexpr int value = 4;
  int num = 12;

  constexpr int f1() const { return 42; }
};

constexpr S s1;

int f() { return 0; }

template<meta::info refl, auto val>
void check_expr_splice_dependent_val() {
  static_assert([:refl:] == val);
}

template<meta::info refl, auto val>
void check_expr_splice_dependent_addr() {
  static_assert(&[:refl:] == val);
}

template<typename T>
void check_expr_splice_dependent_member_ref() {
  static_assert(s1.[: ^T::num :] == 12);
  static_assert(s1.[: ^T::f1 :]() == 42);
}

int main() {
  // static_assert([: ^42 :] == 42); // expected-error

  static_assert([: ^global1 :] == 75);
  check_expr_splice_dependent_val<^global1, 75>();

  static_assert([: ^A :] == 0);
  check_expr_splice_dependent_val<^A, 0>();

  static_assert([: ^S::value :] == 4);
  check_expr_splice_dependent_val<^S::value, 4>();

  static_assert([: ^f :] == &f);
  check_expr_splice_dependent_val<^f, &f>();

  static_assert([: ^f :] == f);
  check_expr_splice_dependent_val<^f, f>();

  static_assert(&[: ^global0 :] == &global0);
  check_expr_splice_dependent_addr<^global0, &global0>();

  static_assert(&[: ^S::value :] == &S::value);
  check_expr_splice_dependent_addr<^S::value, &S::value>();

  // static_assert([: ^s1.num :] == 12); // expected-error

  static_assert(s1.[: ^S::num :] == 12);
  static_assert(s1.[: ^S::f1 :]() == 42);
  check_expr_splice_dependent_member_ref<S>();
}
