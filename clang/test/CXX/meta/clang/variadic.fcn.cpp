// RUN: %clang_cc1 -fsyntax-only -freflection -std=c++17 -verify %s

#define assert(E) if (!(E)) __builtin_abort();
using info = decltype(reflexpr(void));

int f1(int a, int b, int c)
{
  return a + b + c;
}

void call_f1()
{
  static constexpr info v1[] = {reflexpr(1), reflexpr(2), reflexpr(3)};
  static constexpr info v2[] = {reflexpr(1), reflexpr(2)};
  assert(f1(valueof(...v1)) == 6);
  assert(f1(valueof(...v2), 3) == 6);
  assert(f1(3, valueof(...v2)) == 6);
}

template<int a, int b, int c>
int f2()
{
  return a + b + c;
}

void call_f2()
{
  static constexpr info v1[] = {reflexpr(1), reflexpr(2), reflexpr(3)};
  static constexpr info v2[] = {reflexpr(1), reflexpr(2)};
  assert(f2<valueof(...v1)>() == 6);
  int n1 = f2<valueof(...v2), 3>();
  int n2 = f2<3, valueof(...v2)>();
  assert(n1 == 6);
  assert(n2 == 6);
}

template<int a, int b>
int f3(int x, int y)
{
  return a + b + x + y;
}

void call_f3()
{
  static constexpr info v1[] = {reflexpr(1), reflexpr(2)};
  static constexpr info v2[] = {reflexpr(1), reflexpr(2)};
  int n = f3<valueof(...v1)>(valueof(...v2));
  assert(n == 6);
}

template<typename T>
struct S {
  T f4(T a, T b, T c) {
    return a + b + c;
  }
};

void call_f4()
{
  static constexpr info v[] = {reflexpr(1), reflexpr(2), reflexpr(3)};
  S<int> s;

  assert(s.f4(valueof(...v)) == 6);

}

template<typename T>
T f5(T a, T b, T c)
{
  return a + b + c;
}

template<typename T>
void call_f5()
{
  static constexpr info v[] = {reflexpr(T()), reflexpr(T()), reflexpr(T())};
  T n = T() + T() + T();
  assert(f5<T>(valueof(...v)) == n);
}

struct U {
  static int f6(int a, int b, int c) { return a + b + c; }
};

void call_f6()
{
  static constexpr info v[] = {reflexpr(1), reflexpr(2), reflexpr(3)};

  assert(U::f6(valueof(...v)) == 6);
}

void bad_context()
{
  static constexpr info v[] = {reflexpr(int), reflexpr(int), reflexpr(int)};
  f1(typename(...v)); // expected-error {{cannot use typename as reifier in this context: expression expected.}}
}

int main()
{
  call_f1();
  call_f2();
  call_f3();
  call_f4();
  call_f5<int>();
  call_f6();
  bad_context();
}
