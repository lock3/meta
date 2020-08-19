// RUN: %clang_cc1 -freflection -Wno-deprecated-fragment -fsyntax-only -std=c++2a %s

constexpr int f() {
  consteval {
    -> __fragment {
      auto foo = [](int x) {
        ++x;
      };
    };
  }

  return 0;
}
