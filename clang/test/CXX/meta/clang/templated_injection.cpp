// RUN: %clang_cc1 -freflection -std=c++2a %s

#include "reflection_query.h"

template<typename T, auto V>
class test {
  consteval {
    auto ty = reflexpr(T);
    -> fragment struct S {
      static constexpr typename(%{ty}) x = V;
    };
  }
};

test<int, 10> x;
