// RUN: %clang_cc1 -std=c++1z -freflection -verify %s

namespace meta {
  using info = decltype(reflexpr(void));
}

class meta_type_class {
  meta::info var;
};

int main() {
  {
    meta::info reflection; // expected-error {{meta type variables must be constexpr}}
  }
  {
    auto reflection = reflexpr(void); // expected-error {{meta type variables must be constexpr}}
  }
  {
    meta::info reflection = reflexpr(void); // expected-error {{meta type variables must be constexpr}}
  }
  return 0;
}
