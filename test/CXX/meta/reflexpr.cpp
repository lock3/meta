// RUN: %clang_cc1 -I%S/usr/include -I%S/usr/local/include/c++/v1 -std=c++1z -freflection %s

// #include <experimental/meta>

using namespace std::experimental;

struct S { };
enum E { A, B, C };

namespace N {
  namespace M { }
}

void f(int n) {
  constexpr auto ref_S = reflexpr(S);
  constexpr auto ref_E = refelxpr(E);
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


#if 0
namespace Bad {

void g();
void g(int);

template<typename T> void g2(T);

void f() {
  // reflexpr(x); // expected-error {{reflection of undeclared identifier 'x'}}
  // reflexpr(g); // expected-error {{reflection of overloaded identifier 'g'}}
  // reflexpr(g2); // expected-error {{reflection of overloaded identifier 'g2'}}
}

}

namespace OkDependent {

struct product{};
  
template<typename T>
constexpr int test() {
  auto x = reflexpr(T);
  return 0;
}

void test_templates() {
  constexpr int x1 = test<int>();
  constexpr int x2 = test<product>();

}
  
}
#endif
