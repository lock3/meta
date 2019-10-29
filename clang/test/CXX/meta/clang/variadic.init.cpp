// RUN: %clang_cc1 -fsyntax-only -freflection -std=c++17 -verify %s

#define assert(E) if (!(E)) __builtin_abort();
using info = decltype(reflexpr(void));

template<typename T>
struct T1 {
  T1(T a) : a(a) {}

  T a;
};

template<typename T>
struct T2 {
  T2(T b) : b(b) {}

  T b;
};

template<typename T>
struct T3 {
  T3(T c) : c(c) {}

  T c;
};

template<typename T, typename U>
void dependent_init() {
  static constexpr info v[] = {reflexpr(T), reflexpr(U)};

  struct V1 : typename(...v) {
    V1() : typename(...v)(5) {}
  };

  {
    V1 obj;
    assert(obj.a == 5);
    assert(obj.b == 5);
  }

  struct V2 : T, U {
    V2() : typename(...v)(5) {}
  };

  {
    V2 obj;
    assert(obj.a == 5);
    assert(obj.b == 5);
  }
}

template<typename T, typename U, typename V>
void multi_init() {
  static constexpr info v[] = {reflexpr(T), reflexpr(U)};

  struct V1 : V, typename(...v) {
    V1() : V(5), typename(...v)(5) {}
  };

  {
    V1 obj;
    assert(obj.a == 5);
    assert(obj.b == 5);
    assert(obj.c == 5);
  }

  struct V2 : typename(...v), V {
    V2() : typename(...v)(5), V(5) {}
  };

  {
    V2 obj;
    assert(obj.a == 5);
    assert(obj.b == 5);
    assert(obj.c == 5);
  }

  static constexpr info v2[] = {reflexpr(T), reflexpr(U), reflexpr(V)};
  struct V3 : typename(...v), V {
    V3() : typename(...v2)(5){}
  };

  {
    V3 obj;
    assert(obj.a == 5);
    assert(obj.b == 5);
    assert(obj.c == 5);
  }
}

void access_init()
{
  struct V {
    const int b = 10; // expected-note {{member is declared here}}
  };

  struct U {
    const int a = 10; // expected-note {{member is declared here}}
  };

  static constexpr info v[] = {reflexpr(V), reflexpr(U)};

  struct T : private typename(...v) { // expected-note {{declared private here}} expected-note {{constrained by private inheritance here}} expected-note {{declared private here}} expected-note {{constrained by private inheritance here}}
    T() : typename(...v)()
    {}
  };

  T obj;
  if (obj.a == 10) // expected-error {{cannot cast 'T' to its private base class 'U'}} expected-error {{'a' is a private member of 'U'}}
    ;

  if (obj.b == 10) // expected-error {{cannot cast 'T' to its private base class 'V'}} expected-error {{'b' is a private member of 'V'}}
    ;
}

void bad_context_init()
{
  struct U { // expected-note {{candidate constructor (the implicit copy constructor) not viable: no known conversion from 'int' to 'const U' for 1st argument}} expected-note {{candidate constructor (the implicit move constructor) not viable: no known conversion from 'int' to 'U' for 1st argument}} expected-note {{candidate constructor (the implicit default constructor) not viable: requires 0 arguments, but 1 was provided}}
  };

  constexpr info v[] = {reflexpr(U)};

  struct T : typename(...v) {
    T() : typename(...v) {} // expected-error {{expected '('}}

  };

  struct V : typename(...v) {
    V()  : typename(...v)(2) {} // expected-error {{no matching constructor for initialization of 'U'}}

  };

  struct S1 : typename(...v) {
    S1() : idexpr(...v)() {} // expected-error {{cannot use idexpr as reifier in this context: typename expected.}}
  };
  struct S2 : typename(...v) {
    S2() : valueof(...v)() {} // expected-error {{cannot use valueof as reifier in this context: typename expected.}}
  };
  struct S3 : typename(...v) {
    S3() : unqualid(...v)() {} // expected-error {{cannot use unqualid as reifier in this context: typename expected.}}
  };

  struct S4 : idexpr(...v) { // expected-error {{cannot use idexpr as reifier in this context: typename expected.}}
  };
  struct S5 : valueof(...v) { // expected-error {{cannot use valueof as reifier in this context: typename expected.}}
  };
  struct S6 : unqualid(...v) { // expected-error {{cannot use unqualid as reifier in this context: typename expected.}}
  };
}

int main() {
  dependent_init<T1<int>, T2<int>>();
  multi_init<T1<int>, T2<int>, T3<int>>();
  access_init();
  bad_context_init();
}
