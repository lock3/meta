// RUN: %clang_cc1 -fsyntax-only -freflection -std=c++17 -verify %s
// expected-no-diagnostics

#define assert(E) if (!(E)) __builtin_abort();
using info = decltype(reflexpr(void));

struct S1 {
  int a, b, c;
};

void init_S1() {
  static constexpr info v1[] = {reflexpr(1), reflexpr(2), reflexpr(3)};
  static constexpr info v2[] = {reflexpr(1), reflexpr(2)};
  S1 s11 = {valueof(...v1)};
  S1 s12 = {valueof(...v2), 3};
  S1 s13 = {3, valueof(...v2)};

  assert(s11.a == 1 && s11.b == 2 && s11.c == 3);
  assert(s12.a == 1 && s12.b == 2 && s12.c == 3);
  assert(s13.a == 3 && s13.b == 1 && s13.c == 2);
}

template<typename T, typename U>
struct S2 {
  T a;
  U b;

  S2(T a, U b) : a(a), b(b) {}
};

template<typename T, typename U>
void init_S2()
{
  static constexpr info type_range[] = {reflexpr(T), reflexpr(U)};
  static constexpr info value_range[] = {reflexpr(T()), reflexpr(U())};

  S2<typename(...type_range)> s2 = {valueof(...value_range)};

  assert(s2.a == T() && s2.b == U());
}

int main()
{
  init_S1();
  init_S2<int, char>();
}
