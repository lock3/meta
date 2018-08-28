// RUN: %clang_cc1 -I%S/usr/include -I%S/usr/local/include/c++/v1 -std=c++1z -freflection %s

#include <experimental/meta>

using namespace std::experimental;

namespace Ok {

struct S { };
enum E { e1, e2, e3 };

namespace N {
  namespace M { }
}

void f(int n) {
  constexpr meta::info meta1 = reflexpr(n);
  constexpr meta::info meta2 = reflexpr(f);

  double dub = 42.0;
  char character = 'a';
  
  constexpr meta::info meta3 = reflexpr(dub);
  constexpr meta::info meta4 = reflexpr(character);
}

}

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
  meta::info x = reflexpr(T);
  return 0;
}

void test_templates() {
  constexpr int x1 = test<int>();
  constexpr int x2 = test<product>();

}
  
}
