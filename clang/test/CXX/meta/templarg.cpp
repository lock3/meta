// RUN: %clang_cc1 -std=c++2a -freflection -verify %s

template<typename T> // expected-note {{template parameter is declared here}}
struct container {
  T val;
};

template<int DefaultValue> // expected-note {{template parameter is declared here}}
struct defaulted_integer {
  int val = DefaultValue;
};

template<template<typename T> class K, typename T>
struct contained_container {
  K<T> inner_container;
};

struct S { };

void test_non_dependent() {
  {
    constexpr auto reflection = reflexpr(int);

    container<templarg(reflection)> k;
    k.val = 1;
  }

  {
    constexpr auto reflection = reflexpr(S);

    container<templarg(reflection)> k;
    k.val = S();
  }

  {
    constexpr auto reflection = reflexpr(0);

    defaulted_integer<templarg(reflection)> k;
    k.val = 1;
  }

  {
    constexpr auto reflection = reflexpr(container);

    contained_container<templarg(reflection), int> k;
    k.inner_container.val = 1;
  }
}

template<typename R>
void test_dependent_container() {
  {
    constexpr auto reflection = reflexpr(R);

    container<templarg(reflection)> k;
    k.val = R();
  }
}

template<int V>
void test_dependent_default_val() {
  {
    constexpr auto reflection = reflexpr(V);

    defaulted_integer<templarg(reflection)> k;
    k.val = 1;
  }
}

template<template<typename B> class A, typename B>
void test_dependent_container_container() {
  {
    constexpr auto reflection = reflexpr(A);

    contained_container<templarg(reflection), B> k;
    k.inner_container.val = B();
  }
}

void test_dependent() {
  test_dependent_container<int>();
  test_dependent_container<S>();
  test_dependent_default_val<0>();
  test_dependent_container_container<container, int>();
}

void test_bad() {
  {
    constexpr auto reflection = reflexpr(0); // expected-error {{template argument for template type parameter must be a type}}

    container<templarg(reflection)> k;
  }

  {
    constexpr auto reflection = reflexpr(int); // expected-error {{template argument for non-type template parameter must be an expression}}

    defaulted_integer<templarg(reflection)> k;
  }

  {
    constexpr auto reflection = reflexpr(0); // expected-error {{template argument for template template parameter must be a class template or type alias template}}

    contained_container<templarg(reflection), int> k;
  }

  {
    constexpr auto reflection = reflexpr(int); // expected-error {{template argument for template template parameter must be a class template or type alias template}}

    contained_container<templarg(reflection), int> k;
  }
}

int main() {
  test_non_dependent();
  test_dependent();
  test_bad();
  return 0;
}
