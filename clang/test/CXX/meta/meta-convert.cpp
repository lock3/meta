// RUN: %clang_cc1 -freflection -std=c++2a %s

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

namespace N { }

consteval meta::info get_reflection() {
  return reflexpr(N);
}

consteval meta::info get_invalid_reflection() {
  auto refl = get_reflection();
  while (!meta::is_invalid(refl)) {
    refl = meta::get_parent(refl);
  }
  return refl;
}

int main() {
  static_assert(get_reflection());
  static_assert(!get_invalid_reflection());

  return 0;
}
