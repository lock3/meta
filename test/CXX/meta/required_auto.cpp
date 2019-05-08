// RUN: %clang_cc1 -fsyntax-only -freflection -std=c++2a -verify %s

namespace bad {
template<typename T>
void pointers() {
  T *ptr;
  T **ptrptr;
  consteval -> __fragment {
    requires T ptr; // expected-error {{Required declarator not found.}} // expected-note {{required declarator candidate not viable: cannot convert declarator of type ('int' to 'int *')}}
    requires T *ptrptr; // expected-error {{Required declarator not found.}} // expected-note {{required declarator candidate not viable: cannot convert declarator of type ('int *' to 'int **')}}
  };
}

void test() {
  pointers<int>(); // expected-note {{in instantiation of function template specialization 'bad::pointers<int>' requested here}}
pointers<int>();
}
};

namespace good {
#define assert(E) if (!(E)) __builtin_abort();

template<typename T>
void addition(T a, T b)
{
  consteval -> __fragment {
    requires auto a;
    requires auto b;

    assert(a + b == 20);
  };
}

void test() {
  addition(10, 10);
}
};

int main() {
  bad::test();
  good::test();
}
