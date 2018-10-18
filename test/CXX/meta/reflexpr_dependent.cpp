// RUN: %clang_cc1 -std=c++1z -freflection %s

// Check dependent reflexpr and instantiation

template<typename T>
struct S1 {
  constexpr S1() = default;
  constexpr T foo() { return T(); }
  T variable;
};

template<typename T>
int test() {
  constexpr auto x1 = reflexpr(T);

  constexpr T* y1 = nullptr;
  constexpr auto x2 = reflexpr(y1);

  constexpr T y2 = T();
  constexpr auto x3 = reflexpr(y2);

  constexpr const T y3 = T();
  constexpr auto x4 = reflexpr(y3);

  constexpr S1<T> y4 = S1<T>();
  constexpr auto x5 = reflexpr(y4);

  // The ULE this creates is resolving as an implicit call with the current
  // branch code.

  // constexpr auto x6 = reflexpr(S1<T>::foo);
  // constexpr auto x7 = reflexpr(S1<T>::variable);

  // Generate output
  constexpr auto x1_print = ((void) __reflect_print(x1), 0);
  constexpr auto x2_print = ((void) __reflect_print(x2), 0);
  constexpr auto x3_print = ((void) __reflect_print(x3), 0);
  constexpr auto x4_print = ((void) __reflect_print(x4), 0);
  constexpr auto x5_print = ((void) __reflect_print(x5), 0);
  // constexpr auto x6_print = ((void) __reflect_print(x6), 0);
  // constexpr auto x7_print = ((void) __reflect_print(x7), 0);

  return 0;
}

struct S { };

void test_templates() {
  int x1 = test<int>();
  int x2 = test<S>();
}

int main() {
  test_templates();
  return 0;
}
