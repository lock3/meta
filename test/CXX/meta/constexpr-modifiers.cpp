// RUN: %clang_cc1 -verify -freflection -std=c++2a %s

#include "reflection_query.h"
#include "reflection_mod.h"

struct Existing {
  int field_1 = 1;  // expected-error {{cannot make a non-static data member constexpr}}

  int method_1() const {
    return 1;
  }

  ~Existing() { }
};

struct New {
  consteval {
    // Methods
    auto method_1 = reflexpr(Existing::method_1);
    __reflect_mod(query_set_add_constexpr, method_1, true);

    -> method_1;
  }

public:
  constexpr New() { }
};

struct NewBrokenField {
  consteval {
    auto field_1 = reflexpr(Existing::field_1);
    __reflect_mod(query_set_add_constexpr, field_1, true);

    -> field_1;
  }

  constexpr NewBrokenField() { }
};

struct ExistingDestructorOnly {
  ~ExistingDestructorOnly(); // expected-error {{destructor cannot be declared constexpr}}
};


struct NewBrokenDestructor {
  consteval {
    auto refl = reflexpr(ExistingDestructorOnly);

    auto destructor_1 = __reflect(query_get_begin, refl);
    __reflect_mod(query_set_add_constexpr, destructor_1, true);

    -> destructor_1;
  }
};

int main() {
  constexpr New n;

  // Methods
  static_assert(n.method_1() == 1);

  // Fields
  {
    constexpr NewBrokenField new_broken; // expected-error {{constexpr variable 'new_broken' must be initialized by a constant expression}}
    static_assert(new_broken.field_1 == 1);
  }

  // Destructors
  {
    constexpr NewBrokenDestructor new_broken; // expected-error {{constexpr variable 'new_broken' must be initialized by a constant expression}}
  }

  return 0;
}
