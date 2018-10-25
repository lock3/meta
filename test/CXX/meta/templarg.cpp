// RUN: %clang_cc1 -std=c++1z -freflection %s

template<typename T>
struct container {
  T val;
};

template<int DefaultValue>
struct defaulted_integer {
  int val = DefaultValue;
};

// template<template<typename T> class K, typename T>
// struct contained_container {
//   K inner_container;
// };

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

  // {
  //   constexpr auto reflection = reflexpr(container);

  //   contained_container<templarg(reflection), int> k;
  //   k.inner_container.val = 1;
  // }
}

// template<typename R>
// void test_dependent_container() {
//   {
//     constexpr auto reflection = reflexpr(R);

//     container<templarg(reflection)> k;
//     k.val = 1;
//   }
// }

// template<int V>
// void test_dependent_default_val() {
//   {
//     constexpr auto reflection = reflexpr(V);

//     defaulted_integer<templarg(reflection)> k;
//     k.val = 1;
//   }
// }

void test_dependent() {
  // test_dependent_container<int>();
  // test_dependent_container<S>();
  // test_dependent_default_val<0>();
}

int main() {
  test_non_dependent();
  test_dependent();
  return 0;
}
