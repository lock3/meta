// RUN: %clang_cc1 -freflection -std=c++2a %s

#include "reflection_iterator.h"

int foo_base() {
  return 0;
}

constexpr meta::info fn_refl = reflexpr(foo_base);
constexpr meta::info ret_type_refl = __reflect(query_get_return_type, fn_refl);
constexpr meta::range params(fn_refl);

class foo {
  consteval {
    -> fragment struct {
      template<typename T>
      static typename(ret_type_refl) templ_foo(int ignored, -> params) {
        return { };
      }
    };

    -> fragment struct {
      typename(ret_type_refl) (*templ_foo_ptr)(int, -> params);
    };
  }

  foo() {
    consteval {
      -> fragment this {
        templ_foo_ptr = &typename(reflexpr(foo))::template templ_foo<typename(ret_type_refl)>;
      };
    }
  }
};

int main() {
  return 0;
}
