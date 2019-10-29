// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "reflection_query.h"

namespace meta {

using info = decltype(reflexpr(void));

consteval bool is_invalid(info x) {
  return __reflect(query_is_invalid, x);
}

consteval info get_parent(info x) {
  return __reflect(query_get_parent, x);
}

} // end namespace meta

namespace N1 {
  namespace N2 {
    struct S { };
  }
  int x;
}

consteval int nesting(meta::info d) {
  int n = -1;
  meta::info p = meta::get_parent(d);
  while (!meta::is_invalid(p)) {
    p = meta::get_parent(p);
    ++n;
  }
  return n;
}

int main(int argc, char* argv[]) {
  constexpr meta::info n1 = reflexpr(N1);
  constexpr meta::info n2 = reflexpr(N1::N2);
  constexpr meta::info s = reflexpr(N1::N2::S);
  constexpr meta::info x = reflexpr(N1::x);

  static_assert(meta::get_parent(n2) == n1);
  static_assert(meta::get_parent(s) == n2);
  static_assert(meta::get_parent(x) == n1);

  static_assert(nesting(n1) == 0);
  static_assert(nesting(s) == 2);

  return 0;
}
