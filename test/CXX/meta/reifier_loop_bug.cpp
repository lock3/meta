// RUN: %clang_cc1 -freflection -std=c++2a %s

#include "reflection_iterator.h"

namespace meta {
  using info = decltype(reflexpr(void));
}

namespace n {
  constexpr int f() {
    return 1;
  }
}

template<meta::info member>
constexpr void do_thing() {
  static_assert(valueof(member)() == 1);
  static_assert(idexpr(member)() == 1);
}

int main() {
  do_thing<reflexpr(n::f)>();
  static constexpr auto r = meta::range(reflexpr(n));
  template for (constexpr auto member : r) {
    do_thing<member>();
  }
}
