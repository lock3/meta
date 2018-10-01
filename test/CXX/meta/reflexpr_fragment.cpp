// RUN: %clangxx -std=c++1z -freflection %s

#include <experimental/meta>

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
