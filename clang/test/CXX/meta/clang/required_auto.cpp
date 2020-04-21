// RUN: %clang_cc1 -fsyntax-only -freflection -std=c++2a -verify %s

namespace bad {
template<typename T>
void pointers() {
  T *ptr;
  T **ptrptr;
  consteval -> fragment {
    requires T ptr; // expected-error {{Required declarator not found.}} // expected-note {{required declarator candidate not viable: cannot convert declarator of type ('int' to 'int *')}}
    requires T *ptrptr; // expected-error {{Required declarator not found.}} // expected-note {{required declarator candidate not viable: cannot convert declarator of type ('int *' to 'int **')}}
  };
}

template<typename T>
void references(T &ref) {
  consteval -> fragment {
    requires auto ref; // expected-error {{Required conflicting types ('int' vs 'int &')}}
  };
}

void test() {
  pointers<int>(); // expected-note {{in instantiation of function template specialization 'bad::pointers<int>' requested here}}
  int ref;
  references<int>(ref); // expected-note {{in instantiation of function template specialization 'bad::references<int>' requested here}}
}
};

namespace good {
#define assert(E) if (!(E)) __builtin_abort();

template<typename T>
void addition(T a, T b)
{
  consteval -> fragment {
    requires auto a;
    requires auto b;

    assert(a + b == 20);
  };
}

template<typename T>
void reference(T &ref) {
  consteval -> fragment {
    requires auto &ref;
    ref = 10;
  };
}

void test() {
  addition(10, 10);
  int ref;
  reference(ref);
  assert(ref == 10);
}
};

int main() {
  bad::test();
  good::test();
}
