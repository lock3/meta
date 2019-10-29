// RUN: %clang_cc1 -verify -freflection -std=c++2a %s

#include "reflection_query.h"
#include "reflection_mod.h"

struct Existing {
  int method_1() const {
    return 1;
  }

  int method_2() const { // expected-note {{unimplemented pure virtual method 'method_2' in 'New'}}
    return 2;
  }
};

struct New {
  consteval {
    // Methods
    auto method_1 = reflexpr(Existing::method_1);
    __reflect_mod(query_set_add_virtual, method_1, true);

    -> method_1;

    auto method_2 = reflexpr(Existing::method_2);
    __reflect_mod(query_set_add_pure_virtual, method_2, true);

    -> method_2;
  }

  constexpr New() { }
};

struct NewChild : public New {
  constexpr NewChild() { }

  int method_1() const override {
    return -1;
  }

  int method_2() const override {
    return -2;
  }
};

int main() {
  {
    constexpr New n; // expected-error {{variable type 'const New' is an abstract class}}

    // Methods
    static_assert(n.method_1() == 1);
    static_assert(n.method_2() == 2);
  }

  {
    constexpr NewChild n;
  }

  return 0;
}
