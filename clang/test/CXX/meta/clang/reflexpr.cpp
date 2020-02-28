// RUN: %clang_cc1 -std=c++2a -freflection -verify %s

struct S { };
enum E { A, B, C };

namespace N {
  namespace M { }
}

void f(int n) {
  constexpr auto ref_S = reflexpr(S);
  constexpr auto ref_E = reflexpr(E);
  constexpr auto ref_EA = reflexpr(A);
  constexpr auto ref_N =  reflexpr(N);
  constexpr auto ref_NM = reflexpr(N::M);

  constexpr auto meta1 = reflexpr(n);
  constexpr auto meta2 = reflexpr(f);

  double dub = 42.0;
  char character = 'a';

  constexpr auto meta3 = reflexpr(dub);
  constexpr auto meta4 = reflexpr(character);
}

namespace Bad {

void g();
void g(int);

template<typename T> void g2(T);

void f() {
  auto a = reflexpr(x); // expected-error {{use of undeclared identifier 'x'}}
#if 0
  auto b = reflexpr(g); // expected-error {{reflection of overloaded identifier 'g'}}
  auto c = reflexpr(g2); // expected-error {{reflection of overloaded identifier 'g2'}}
#endif
}

}

namespace OkDependent {

struct product{};

template<typename T>
consteval int test() {
  auto x = reflexpr(T);
  return 0;
}

void test_templates() {
  constexpr int x1 = test<int>();
  constexpr int x2 = test<product>();

}

}
