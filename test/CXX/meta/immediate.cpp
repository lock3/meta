// RUN: %clang_cc1 -fsyntax-only -verify -std=c++1z %s

immediate class C1 {}; // expected-error {{class cannot be marked 'immediate'}}
immediate struct S1 {}; // expected-error {{struct cannot be marked 'immediate'}}
immediate union U1 {}; // expected-error {{union cannot be marked 'immediate'}}
immediate enum E1 {}; // expected-error {{enum cannot be marked 'immediate'}}

template <typename T> immediate class TC1 {}; // expected-error {{class cannot be marked 'immediate'}}
template <typename T> immediate struct TS1 {}; // expected-error {{struct cannot be marked 'immediate'}}
template <typename T> immediate union TU1 {}; // expected-error {{union cannot be marked 'immediate'}}

immediate class C2; // expected-error {{class cannot be marked 'immediate'}}
immediate struct S2; // expected-error {{struct cannot be marked 'immediate'}}
immediate union U2; // expected-error {{union cannot be marked 'immediate'}}

template <typename T> immediate class TC2; // expected-error {{class cannot be marked 'immediate'}}
template <typename T> immediate struct TS2; // expected-error {{struct cannot be marked 'immediate'}}
template <typename T> immediate union TU2; // expected-error {{union cannot be marked 'immediate'}}

class C2 {} immediate; // expected-error {{class cannot be marked 'immediate'}}
struct S2 {} immediate; // expected-error {{struct cannot be marked 'immediate'}}
union U2 {} immediate; // expected-error {{union cannot be marked 'immediate'}}
enum E2 {} immediate; // expected-error {{enum cannot be marked 'immediate'}}

immediate int x = 0; // expected-error {{variable cannot be marked 'immediate'}}

struct S3 {
  immediate ~S3() { } // expected-error {{destructor cannot be marked 'immediate'}}
};

immediate int f1() { return 0; }
immediate constexpr int f2() { return 0; }
constexpr immediate int f3() { return 0; }
immediate immediate int f4() { return 0; } // expected-warning {{duplicate 'immediate' declaration specifier}}

struct S4 {
  static immediate int f1() { return 1; }
};

namespace Ok {
  struct S {
    int a, b;
  };

  immediate int f1(int n) { return n + 5; }
  immediate S f2(int a, int b) { return {a, b}; }


  // FIXME: Actually verify the output.
  void g() {
    int x = f1(3);
    S s = f2(5, 6);
  }
}


namespace Bad {
  immediate int f(int n) { return n; }

  struct S {
    int n;
    immediate int f(int m) { 
      return m + n; // expected-note {{read of non-constexpr variable 's2'}}
    }
  };

  template<typename T>
  immediate T f2(T n) { return n + 1; }

  template<typename T>
  struct S2 {
    immediate T f(T n) { 
      return n + k; // expected-note {{read of non-constexpr variable 's3'}}
    }

    immediate static T g(T n) { return n + 1; }

    int k;
  };


  void test(int n) { // expected-note {{declared here}}
    int x = 0; // expected-note {{declared here}} 
    f(x); // expected-error {{cannot evaluate call to immediate function}} \
          // expected-note {{read of non-const variable 'x' is not allowed in a constant expression}}

    int y = 1; // expected-note {{declared here}}
    S s1 {3};
    s1.f(y); // expected-error {{cannot evaluate call to immediate member function}} \
             // expected-note {{read of non-const variable 'y' is not allowed in a constant expression}}

    S s2 {3}; // expected-note {{declared here}}
    s2.f(0); // expected-error {{cannot evaluate call to immediate member function}} \
             // expected-note {{in call to '&s2->f(0)'}}

    f2(n + 1); // expected-error {{cannot evaluate call to immediate function}} \
               // expected-note {{read of non-const variable 'n' is not allowed in a constant expression}}

    S2<int> s3{n}; // expected-note {{declared here}}
    s3.f(0); // expected-error {{cannot evaluate call to immediate member function}} \
             // expected-note {{in call to '&s3->f(0)'}}
  }
}


void test(int n) {
}

