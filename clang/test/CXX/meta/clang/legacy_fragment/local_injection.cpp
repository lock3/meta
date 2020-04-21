// RUN: %clang_cc1 -freflection -Wno-deprecated-fragment -std=c++2a -fsyntax-only -verify %s -Wmissing-noreturn

consteval int f1() {
  consteval {
    -> __fragment {
      return 42;
    };
  };
}

consteval int f2() { // expected-error {{no return statement in consteval function}}
  consteval {
    -> __fragment {
      int x = 10;
    };
  };
}

constexpr int f3() {
  consteval {
    -> __fragment {
      constexpr int x = 10;
      if constexpr (x == 10)
        return x;
      else
        return 1;
    };
  };
}

template<int Y>
constexpr int f4() {
  consteval {
    -> __fragment {
      return Y;
    };
  }
}

int main() {
  static_assert(f1() == 42);
  static_assert(f3() == 10);
  static_assert(f4<12>() == 12);
}
