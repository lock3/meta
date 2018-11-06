// RUN: %clang_cc1 -std=c++1z -freflection %s

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

template<typename T>
void foo() {
  static_assert(s1.idexpr(reflexpr(T::num)) == 12);
  static_assert(s1.idexpr(reflexpr(T::f1))() == 42);
}

int main() {
  // static_assert(idexpr(reflexpr(42)) == 42); // expected-error

  static_assert(&idexpr(reflexpr(global0)) == &global0);
  static_assert(idexpr(reflexpr(global1)) == 75);
  
  static_assert(idexpr(reflexpr(A)) == 0);
  static_assert(idexpr(reflexpr(S::value)) == 4);
  static_assert(&idexpr(reflexpr(S::value)) == &S::value);
  
  // static_assert(idexpr(reflexpr(s1.num)) == 12); // expected-error
  
  static_assert(idexpr(reflexpr(f)) == &f);
  static_assert(idexpr(reflexpr(f)) == f);
  
  static_assert(s1.idexpr(reflexpr(S::num)) == 12);
  static_assert(s1.idexpr(reflexpr(S::f1))() == 42);

  foo<S>();
}
