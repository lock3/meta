// RUN: %clang_cc1 -freflection -std=c++2a %s

#include "reflection_iterator.h"

namespace meta {
  using info = decltype(^void);
}

namespace n {
  constexpr int f() {
    return 1;
  }
}

template<meta::info member>
constexpr void do_thing() {
  static_assert([:member:]() == 1);
}

int main() {
  do_thing<^n::f>();
  static constexpr auto r = meta::range(^n);
  template for (constexpr auto member : r) {
    do_thing<member>();
  }
}
