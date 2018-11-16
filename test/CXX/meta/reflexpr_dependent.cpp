// RUN: %clang_cc1 -std=c++1z -freflection %s

// Check dependent reflexpr and instantiation

template<typename T>
struct S1 {
  constexpr S1() = default;
  constexpr T foo() { return T(); }
  T variable;

  void deleted_function() = delete;
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

  constexpr auto x6 = reflexpr(S1<T>::foo);
  constexpr auto x7 = reflexpr(S1<T>::variable);

  constexpr auto x8 = reflexpr(S1<T>::deleted_function);

  // Generate output
  constexpr auto x1_print = __reflect_pretty_print(x1);
  constexpr auto x2_print = __reflect_pretty_print(x2);
  constexpr auto x3_print = __reflect_pretty_print(x3);
  constexpr auto x4_print = __reflect_pretty_print(x4);
  constexpr auto x5_print = __reflect_pretty_print(x5);
  constexpr auto x6_print = __reflect_pretty_print(x6);
  constexpr auto x7_print = __reflect_pretty_print(x7);
  constexpr auto x8_print = __reflect_pretty_print(x8);

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
