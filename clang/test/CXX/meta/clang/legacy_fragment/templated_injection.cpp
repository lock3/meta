// RUN: %clang_cc1 -freflection -Wno-deprecated-fragment -std=c++2a %s

#include "../reflection_query.h"

template<typename T, auto V>
class test {
  consteval {
    auto ty = reflexpr(T);
    -> __fragment struct S {
      static constexpr typename(ty) x = V;
    };
  }
};

test<int, 10> x;
