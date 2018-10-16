// RUN: %clang_cc1 -I%S/usr/include -I%S/usr/local/include/c++/v1 -std=c++1z -freflection %s

// Check dependent reflexpr and instantiation

template<typename T>
struct S1 {
  constexpr S1() = default;
  constexpr T foo() { return T(); }
  T variable;
};

template<typename T>
int test() {
  // constexpr auto x1 = reflexpr(T);

  // constexpr T* y1 = nullptr;
  // constexpr auto x2 = reflexpr(y1);

  // constexpr T y2 = T();
  // constexpr auto x3 = reflexpr(y2);

  // constexpr const T y3 = T();
  // constexpr auto x4 = reflexpr(y3);

  // constexpr S1<T> y4 = S1<T>();
  // constexpr auto x5 = reflexpr(y4);

  // FIX ME: compiler registers this method reflection as
  // not constexpr
  constexpr auto x6 = reflexpr(S1<T>::foo);
  constexpr auto x7 = reflexpr(S1<T>::variable);

  // Generate output
  // (void)__reflect_print(x1);
  // (void)__reflect_print(x2);
  // (void)__reflect_print(x3);
  // (void)__reflect_print(x4);
  // (void)__reflect_print(x5);
  // (void)__reflect_print(x6);
  // (void)__reflect_print(x7);
  return 0;
}

struct S { };

void test_templates() {
  int x1 = test<int>();
  int x2 = test<S>();
}

int main(int argc, char* argv[])
{
  test_templates();
}
