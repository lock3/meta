// RUN: %clang_cc1 -std=c++1z -freflection %s
// expected-no-diagnostics

#define assert(E) if (!(E)) __builtin_abort();

struct record {
  int a, b, c;

  struct nest {
    int irrelevant1;
    double irrelevant2;
  };

  double d;
  int e;
  float f;

  void function() {}

  int g;

  constexpr record(int a, int b, int c, double d, int e, float f, int g)
    : a(a), b(b), c(c), d(d), e(e), f(f), g(g)
    { }
};

constexpr void select_record() {
  constexpr record r(0, 1, 2, 3, 4, 5, 6);
  static_assert(__select_member(r, 0) == 0);
  static_assert(__select_member(r, 1) == 1);
  static_assert(__select_member(r, 2) == 2);
  static_assert(__select_member(r, 3) == 3);
  static_assert(__select_member(r, 4) == 4);
  static_assert(__select_member(r, 5) == 5);
  static_assert(__select_member(r, 6) == 6);
}

template <typename T, typename U, typename S>
struct dependent_record {
  T a, b, c;

  struct nest {
    T irrelevant1;
    U irrelevant2;
  };

  U d;
  T e;
  S f;

  void function() {}

  T g;

  constexpr dependent_record(T a, T b, T c, U d, T e, S f, T g)
    : a(a), b(b), c(c), d(d), e(e), f(f), g(g)
    { }
};


template <typename T, typename U, typename S>
void select_dependent_record() {
  dependent_record<T, U, S> r(0, 1, 2, 3, 4, 5, 6);
  assert(__select_member(r, 0) == 0);
  assert(__select_member(r, 1) == 1);
  assert(__select_member(r, 2) == 2);
  assert(__select_member(r, 3) == 3);
  assert(__select_member(r, 4) == 4);
  assert(__select_member(r, 5) == 5);
  assert(__select_member(r, 6) == 6);
}

void select_constexpr() {
  static constexpr record r(0, 1, 2, 3, 4, 5, 6);
  static_assert(__select_member(r, 0) == 0);
  static_assert(__select_member(r, 1) == 1);
  static_assert(__select_member(r, 2) == 2);
  static_assert(__select_member(r, 3) == 3);
  static_assert(__select_member(r, 4) == 4);
  static_assert(__select_member(r, 5) == 5);
  static_assert(__select_member(r, 6) == 6);
}

template<typename T, typename U, typename S>
void select_dependent_constexpr() {
  static constexpr dependent_record<T, U, S> r(0, 1, 2, 3, 4, 5, 6);
  static_assert(__select_member(r, 0) == 0);
  static_assert(__select_member(r, 1) == 1);
  static_assert(__select_member(r, 2) == 2);
  static_assert(__select_member(r, 3) == 3);
  static_assert(__select_member(r, 4) == 4);
  static_assert(__select_member(r, 5) == 5);
  static_assert(__select_member(r, 6) == 6);
}

int main() {
  select_record();
  select_dependent_record<double, int, float>();
  select_constexpr();
  select_dependent_constexpr<double, int, float>();
}
