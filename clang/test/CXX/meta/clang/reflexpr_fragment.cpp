// RUN: %clang_cc1 -freflection -std=c++2a %s

struct ExistingClass {
  int inner = 10;

  constexpr ExistingClass() = default;
};

class NewClass {
  consteval {
    -> reflexpr(ExistingClass::inner);
  }
};

int main() {
  static_assert(NewClass().inner == 10);
  return 0;
}
