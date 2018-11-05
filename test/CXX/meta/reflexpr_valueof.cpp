// RUN: %clang_cc1 -std=c++1z -freflection %s

const int global1 = 75;
constexpr int global2 = 76;

enum E { A, B, C };

struct S {
  static constexpr int value = 4;
  int num = 12;
};

constexpr S s1;

int f() { return 0; }

int main() {
  static_assert(valueof(reflexpr(42)) == 42);
  static_assert(valueof(reflexpr(global1)) == 75);
  static_assert(valueof(reflexpr(global2)) == 76);
  static_assert(valueof(reflexpr(A)) == 0);
  static_assert(valueof(reflexpr(S::value)) == 4);
  static_assert(valueof(reflexpr(s1.num)) == 12);
  static_assert(valueof(reflexpr(f)) == &f);
  static_assert(valueof(reflexpr(S::num)) == &S::num);
}
