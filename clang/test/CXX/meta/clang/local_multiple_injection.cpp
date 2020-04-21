// RUN: %clang_cc1 -freflection -std=c++2a %s

constexpr int count_to_five() {
  int k = 0;

  consteval {
    for (int i = 1; i <= 5; ++i) {
      -> fragment {
        requires int k;

        ++k;
      };
    }
  }

  return k;
}

static_assert(count_to_five() == 5);
