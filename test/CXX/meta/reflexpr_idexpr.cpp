// RUN: %clang_cc1 -std=c++2a -freflection %s

namespace meta {
  using info = decltype(reflexpr(void));
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
void check_idexpr_dependent_val() {
  static_assert(idexpr(refl) == val);
}

template<meta::info refl, auto val>
void check_idexpr_dependent_addr() {
  static_assert(&idexpr(refl) == val);
}

template<typename T>
void check_idexpr_dependent_member_ref() {
  static_assert(s1.idexpr(reflexpr(T::num)) == 12);
  static_assert(s1.idexpr(reflexpr(T::f1))() == 42);
}

int main() {
  // static_assert(idexpr(reflexpr(42)) == 42); // expected-error

  static_assert(idexpr(reflexpr(global1)) == 75);
  check_idexpr_dependent_val<reflexpr(global1), 75>();

  static_assert(idexpr(reflexpr(A)) == 0);
  check_idexpr_dependent_val<reflexpr(A), 0>();

  static_assert(idexpr(reflexpr(S::value)) == 4);
  check_idexpr_dependent_val<reflexpr(S::value), 4>();

  static_assert(idexpr(reflexpr(f)) == &f);
  check_idexpr_dependent_val<reflexpr(f), &f>();

  static_assert(idexpr(reflexpr(f)) == f);
  check_idexpr_dependent_val<reflexpr(f), f>();

  static_assert(&idexpr(reflexpr(global0)) == &global0);
  check_idexpr_dependent_addr<reflexpr(global0), &global0>();

  static_assert(&idexpr(reflexpr(S::value)) == &S::value);
  check_idexpr_dependent_addr<reflexpr(S::value), &S::value>();

  // static_assert(idexpr(reflexpr(s1.num)) == 12); // expected-error

  static_assert(s1.idexpr(reflexpr(S::num)) == 12);
  static_assert(s1.idexpr(reflexpr(S::f1))() == 42);
  check_idexpr_dependent_member_ref<S>();
}
