// RUN: %clang_cc1 -std=c++2a -freflection -verify %s

struct S { };
enum E { A, B, C };

namespace N {
  namespace M { }
}

void f(int n) {
  constexpr auto ref_S = ^S;
  constexpr auto ref_E = ^E;
  constexpr auto ref_EA = ^A;
  constexpr auto ref_N =  ^N;
  constexpr auto ref_NM = ^N::M;

  constexpr auto meta1 = ^n;
  constexpr auto meta2 = ^f;

  double dub = 42.0;
  char character = 'a';

  constexpr auto meta3 = ^dub;
  constexpr auto meta4 = ^character;
}

namespace Bad {

void g();
void g(int);

template<typename T> void g2(T);

void f() {
  auto a = ^x; // expected-error {{use of undeclared identifier 'x'}}
#if 0
  auto b = ^g; // expected-error {{reflection of overloaded identifier 'g'}}
  auto c = ^g2; // expected-error {{reflection of overloaded identifier 'g2'}}
#endif
}

}

namespace OkDependent {

struct product{};

template<typename T>
consteval int test() {
  auto x = ^T;
  return 0;
}

void test_templates() {
  constexpr int x1 = test<int>();
  constexpr int x2 = test<product>();

}

}
