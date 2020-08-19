// RUN: %clang_cc1 -verify -freflection -std=c++2a %s

#include "reflection_query.h"
#include "reflection_mod.h"

namespace make_constexpr {

namespace existing {
  int variable_1 = 0;

  void function_1() { }
}

consteval {
  auto variable_1 = reflexpr(existing::variable_1);
  __reflect_mod(query_set_constexpr, variable_1, ConstexprModifier::Constexpr);

  -> variable_1;
}

consteval {
  auto function_1 = reflexpr(existing::function_1);
  __reflect_mod(query_set_constexpr, function_1, ConstexprModifier::Constexpr);

  -> function_1;
}

struct Existing {
  int field_1 = 1;  // expected-error {{cannot make a non-static data member constexpr}}

  int method_1() const {
    return 1;
  }
};

struct NewMethod {
  consteval {
    auto method_1 = reflexpr(Existing::method_1);
    __reflect_mod(query_set_constexpr, method_1, ConstexprModifier::Constexpr);

    -> method_1;
  }

public:
  constexpr NewMethod() { }
};

struct NewBrokenField {
  consteval {
    auto field_1 = reflexpr(Existing::field_1);
    __reflect_mod(query_set_constexpr, field_1, ConstexprModifier::Constexpr);

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
    __reflect_mod(query_set_constexpr, destructor_1, ConstexprModifier::Constexpr);

    -> destructor_1;
  }
};

} // end namespace make_constexpr

namespace make_consteval {

namespace existing {
  int variable_1 = 0; // expected-error {{cannot make a variable consteval}}

  void function_1() { }
}

consteval {
  auto variable_1 = reflexpr(existing::variable_1);
  __reflect_mod(query_set_constexpr, variable_1, ConstexprModifier::Consteval);

  -> variable_1;
}

consteval {
  auto function_1 = reflexpr(existing::function_1);
  __reflect_mod(query_set_constexpr, function_1, ConstexprModifier::Consteval);

  -> function_1;
}

struct Existing {
  int field_1 = 1;  // expected-error {{cannot make a non-static data member consteval}}

  int method_1() const {
    return 1;
  }
};

struct NewMethod {
  consteval {
    auto method_1 = reflexpr(Existing::method_1);
    __reflect_mod(query_set_constexpr, method_1, ConstexprModifier::Consteval);

    -> method_1;
  }

public:
  consteval NewMethod() { }
};

struct NewBrokenField {
  consteval {
    auto field_1 = reflexpr(Existing::field_1);
    __reflect_mod(query_set_constexpr, field_1, ConstexprModifier::Consteval);

    -> field_1;
  }

  constexpr NewBrokenField() { }
};

struct ExistingDestructorOnly {
  ~ExistingDestructorOnly(); // expected-error {{destructor cannot be declared consteval}}
};

struct NewBrokenDestructor {
  consteval {
    auto refl = reflexpr(ExistingDestructorOnly);

    auto destructor_1 = __reflect(query_get_begin, refl);
    __reflect_mod(query_set_constexpr, destructor_1, ConstexprModifier::Consteval);

    -> destructor_1;
  }
};

} // end namespace make_consteval

namespace make_constinit {

namespace existing {
  int variable_1 = 0;

  void function_1() { } // expected-error {{cannot make a function constinit}}
}

consteval {
  auto variable_1 = reflexpr(existing::variable_1);
  __reflect_mod(query_set_constexpr, variable_1, ConstexprModifier::Constinit);

  -> variable_1;
}

consteval {
  auto function_1 = reflexpr(existing::function_1);
  __reflect_mod(query_set_constexpr, function_1, ConstexprModifier::Constinit);

  -> function_1;
}

struct Existing {
  int field_1 = 1;  // expected-error {{cannot make a non-static data member constinit}}

  int method_1() const {  // expected-error {{cannot make a function constinit}}
    return 1;
  }
};

struct NewBrokenMethod {
  consteval {
    auto method_1 = reflexpr(Existing::method_1);
    __reflect_mod(query_set_constexpr, method_1, ConstexprModifier::Constinit);

    -> method_1;
  }

public:
  consteval NewBrokenMethod() { }
};

struct NewBrokenField {
  consteval {
    auto field_1 = reflexpr(Existing::field_1);
    __reflect_mod(query_set_constexpr, field_1, ConstexprModifier::Constinit);

    -> field_1;
  }

  constexpr NewBrokenField() { }
};

struct ExistingDestructorOnly {
  ~ExistingDestructorOnly(); // expected-error {{cannot make a function constinit}}
};

struct NewBrokenDestructor {
  consteval {
    auto refl = reflexpr(ExistingDestructorOnly);

    auto destructor_1 = __reflect(query_get_begin, refl);
    __reflect_mod(query_set_constexpr, destructor_1, ConstexprModifier::Constinit);

    -> destructor_1;
  }
};

} // end namespace make_constinit

int main() {
  return 0;
}

