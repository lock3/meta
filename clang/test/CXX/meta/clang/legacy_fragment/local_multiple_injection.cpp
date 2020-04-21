// RUN: %clang_cc1 -freflection -Wno-deprecated-fragment -std=c++2a %s

constexpr int count_to_five() {
  int k = 0;

  consteval {
    for (int i = 1; i <= 5; ++i) {
      -> __fragment {
        requires int k;

        ++k;
      };
    }
  }

  return k;
}

static_assert(count_to_five() == 5);
