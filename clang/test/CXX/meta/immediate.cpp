// RUN: %clang_cc1 -std=c++2a -freflection -fsyntax-only -verify %s

consteval class C1 {}; // expected-error {{class cannot be marked consteval}}
consteval struct S1 {}; // expected-error {{struct cannot be marked consteval}}
consteval union U1 {}; // expected-error {{union cannot be marked consteval}}
consteval enum E1 {}; // expected-error {{enum cannot be marked consteval}}

template <typename T> consteval class TC1 {}; // expected-error {{class cannot be marked consteval}}
template <typename T> consteval struct TS1 {}; // expected-error {{struct cannot be marked consteval}}
template <typename T> consteval union TU1 {}; // expected-error {{union cannot be marked consteval}}

consteval class C2; // expected-error {{class cannot be marked consteval}}
consteval struct S2; // expected-error {{struct cannot be marked consteval}}
consteval union U2; // expected-error {{union cannot be marked consteval}}

template <typename T> consteval class TC2; // expected-error {{class cannot be marked consteval}}
template <typename T> consteval struct TS2; // expected-error {{struct cannot be marked consteval}}
template <typename T> consteval union TU2; // expected-error {{union cannot be marked consteval}}

class C2 {} consteval; // expected-error {{class cannot be marked consteval}}
struct S2 {} consteval; // expected-error {{struct cannot be marked consteval}}
union U2 {} consteval; // expected-error {{union cannot be marked consteval}}
enum E2 {} consteval; // expected-error {{enum cannot be marked consteval}}

consteval int x = 0; // expected-error {{consteval can only be used in function declarations}}

struct S3 {
  consteval ~S3() { } // expected-error {{destructor cannot be declared consteval}}
};

consteval int f1() { return 0; }
consteval constexpr int f2() { return 0; } // expected-error {{cannot combine with previous 'consteval' declaration specifier}}
constexpr consteval int f3() { return 0; } // expected-error {{cannot combine with previous 'constexpr' declaration specifier}}
consteval consteval int f4() { return 0; } // expected-warning {{duplicate 'consteval' declaration specifier}}

struct S4 {
  static consteval int f1() { return 1; }
};

namespace Ok {
  struct S {
    int a, b;
  };

  consteval int f1(int n) { return n + 5; }
  consteval S f2(int a, int b) { return {a, b}; }


  // FIXME: Actually verify the output.
  void g() {
    int x = f1(3);
    S s = f2(5, 6);
  }
}


namespace Bad {
  consteval int f(int n) { return n; }

  struct S {
    int n;
    consteval int f(int m) {
      return m + n; // expected-note {{read of non-constexpr variable 's2'}}
    }
  };

  template<typename T>
  consteval T f2(T n) { return n + 1; }

  template<typename T>
  struct S2 {
    consteval T f(T n) {
      return n + k; // expected-note {{read of non-constexpr variable 's3'}}
    }

    consteval static T g(T n) { return n + 1; }

    int k;
  };


  void test(int n) { // expected-note {{declared here}}
    int x = 0; // expected-note {{declared here}}
    f(x); // expected-error {{call to consteval function 'Bad::f' is not a constant expression}} \
          // expected-note {{read of non-const variable 'x' is not allowed in a constant expression}} \
          // expected-warning {{expression result unused}}

    int y = 1; // expected-note {{declared here}}
    S s1 {3};
    s1.f(y); // expected-error {{call to consteval function 'Bad::S::f' is not a constant expression}} \
             // expected-note {{read of non-const variable 'y' is not allowed in a constant expression}} \
             // expected-warning {{expression result unused}}

    S s2 {3}; // expected-note {{declared here}}
    s2.f(0); // expected-error {{call to consteval function 'Bad::S::f' is not a constant expression}} \
             // expected-note {{in call to '&s2->f(0)'}} \
             // expected-warning {{expression result unused}}

    f2(n + 1); // expected-error {{call to consteval function 'Bad::f2<int>' is not a constant expression}} \
               // expected-note {{read of non-const variable 'n' is not allowed in a constant expression}} \
               // expected-warning {{expression result unused}}

    S2<int> s3{n}; // expected-note {{declared here}}
    s3.f(0); // expected-error {{call to consteval function 'Bad::S2<int>::f' is not a constant expression}} \
             // expected-note {{in call to '&s3->f(0)'}} \
             // expected-warning {{expression result unused}}
  }
}


void test(int n) {
}
