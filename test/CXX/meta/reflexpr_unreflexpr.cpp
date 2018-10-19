// RUN: %clang_cc1 -std=c++1z -freflection %s

int main() {
  typename(reflexpr(int)) i = unreflexpr(reflexpr(42));
  return 0;
}
