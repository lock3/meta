// RUN: %clang_cc1 -freflection -std=c++1z %s

#define assert(E) if (!(E)) __builtin_abort();

struct ExistingClass {
  int inner = 10;
};

class NewClass {
  constexpr {
    -> reflexpr(ExistingClass);
  }
};

int main() {
  NewClass n;
  assert(n.inner == 10);
  return 0;
}
