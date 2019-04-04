// RUN: %clang_cc1 -freflection -std=c++1z %s
// expected-no-diagnostics

#define assert(E) if (!(E)) __builtin_abort();

// test injection in a non-constexpr function
int f() {
  consteval -> __fragment {
    return 9;
  }
}

int g() {
  constexpr auto frag1 = __fragment {
    return 9;
  };

  consteval -> frag1;
}

constexpr auto frag = __fragment {
};

template<typename T>
T h() {
  consteval -> __fragment {
    return T();
  };
}

int main() {
  // MetaprogramDecl
  consteval {
    -> frag;
  };
  // InjectionDecl
  consteval -> frag;

  assert(f() == 9);
  assert(g() == 9);
  assert(h<int>() == int());
}
