// RUN: %clang_cc1 -std=c++1z -freflection -fsyntax-only -verify %s

constexpr! class C1 {}; // expected-error {{class cannot be marked 'constexpr!'}}
constexpr! struct S1 {}; // expected-error {{struct cannot be marked 'constexpr!'}}
constexpr! union U1 {}; // expected-error {{union cannot be marked 'constexpr!'}}
constexpr! enum E1 {}; // expected-error {{enum cannot be marked 'constexpr!'}}

template <typename T> constexpr! class TC1 {}; // expected-error {{class cannot be marked 'constexpr!'}}
template <typename T> constexpr! struct TS1 {}; // expected-error {{struct cannot be marked 'constexpr!'}}
template <typename T> constexpr! union TU1 {}; // expected-error {{union cannot be marked 'constexpr!'}}

constexpr! class C2; // expected-error {{class cannot be marked 'constexpr!'}}
constexpr! struct S2; // expected-error {{struct cannot be marked 'constexpr!'}}
constexpr! union U2; // expected-error {{union cannot be marked 'constexpr!'}}

template <typename T> constexpr! class TC2; // expected-error {{class cannot be marked 'constexpr!'}}
template <typename T> constexpr! struct TS2; // expected-error {{struct cannot be marked 'constexpr!'}}
template <typename T> constexpr! union TU2; // expected-error {{union cannot be marked 'constexpr!'}}

class C2 {} constexpr!; // expected-error {{class cannot be marked 'constexpr!'}}
struct S2 {} constexpr!; // expected-error {{struct cannot be marked 'constexpr!'}}
union U2 {} constexpr!; // expected-error {{union cannot be marked 'constexpr!'}}
enum E2 {} constexpr!; // expected-error {{enum cannot be marked 'constexpr!'}}

constexpr! int x = 0; // expected-error {{variable cannot be marked 'constexpr!'}}

struct S3 {
  constexpr! ~S3() { } // expected-error {{destructor cannot be marked 'constexpr!'}}
};

constexpr! int f1() { return 0; }
constexpr! constexpr int f2() { return 0; } // expected-error {{cannot combine with previous 'constexpr!' declaration specifier}}
constexpr constexpr! int f3() { return 0; } // expected-error {{cannot combine with previous 'constexpr' declaration specifier}}
constexpr! constexpr! int f4() { return 0; } // expected-warning {{duplicate 'constexpr!' declaration specifier}}

struct S4 {
  static constexpr! int f1() { return 1; }
};

namespace Ok {
  struct S {
    int a, b;
  };

  constexpr! int f1(int n) { return n + 5; }
  constexpr! S f2(int a, int b) { return {a, b}; }


  // FIXME: Actually verify the output.
  void g() {
    int x = f1(3);
    S s = f2(5, 6);
  }
}


namespace Bad {
  constexpr! int f(int n) { return n; }

  struct S {
    int n;
    constexpr! int f(int m) { 
      return m + n; // expected-note {{read of non-constexpr variable 's2'}}
    }
  };

  template<typename T>
  constexpr! T f2(T n) { return n + 1; }

  template<typename T>
  struct S2 {
    constexpr! T f(T n) { 
      return n + k; // expected-note {{read of non-constexpr variable 's3'}}
    }

    constexpr! static T g(T n) { return n + 1; }

    int k;
  };


  void test(int n) { // expected-note {{declared here}}
    int x = 0; // expected-note {{declared here}} 
    f(x); // expected-error {{cannot evaluate call to constexpr! function}} \
          // expected-note {{read of non-const variable 'x' is not allowed in a constant expression}}

    int y = 1; // expected-note {{declared here}}
    S s1 {3};
    s1.f(y); // expected-error {{cannot evaluate call to constexpr! member function}} \
             // expected-note {{read of non-const variable 'y' is not allowed in a constant expression}}

    S s2 {3}; // expected-note {{declared here}}
    s2.f(0); // expected-error {{cannot evaluate call to constexpr! member function}} \
             // expected-note {{in call to '&s2->f(0)'}}

    f2(n + 1); // expected-error {{cannot evaluate call to constexpr! function}} \
               // expected-note {{read of non-const variable 'n' is not allowed in a constant expression}}

    S2<int> s3{n}; // expected-note {{declared here}}
    s3.f(0); // expected-error {{cannot evaluate call to constexpr! member function}} \
             // expected-note {{in call to '&s3->f(0)'}}
  }
}


void test(int n) {
}

