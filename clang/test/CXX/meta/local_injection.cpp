// RUN: %clang_cc1 -freflection -std=c++2a -fsyntax-only -verify %s -Wmissing-noreturn

consteval int f1() {
  consteval {
    -> fragment {
      return 42;
    };
  };
}

consteval int f2() { // expected-error {{no return statement in consteval function}}
  consteval {
    -> fragment {
      int x = 10;
    };
  };
}

constexpr int f3() {
  consteval {
    -> fragment {
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
    -> fragment {
      return Y;
    };
  }
}

int main() {
  static_assert(f1() == 42);
  static_assert(f3() == 10);
  static_assert(f4<12>() == 12);
}
