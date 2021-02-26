// RUN: %clang -freflection -std=c++2a %s

#include "reflection_query.h"

consteval auto name_of(meta::info refl) {
  return __reflect(query_get_name, refl);
}

template<typename T>
class foo {
  consteval {
    ([](auto cap_ty, auto cap_name) consteval {
      -> fragment struct {
        typename [: %{cap_ty} :] [# %{cap_name} #]() {
          return 0;
        }
      };
     })(^T, "foo");
  }
};

int main() {
  foo<int> f;
  return f.foo();
}
