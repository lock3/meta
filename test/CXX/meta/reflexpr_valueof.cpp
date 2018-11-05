// RUN: %clang_cc1 -std=c++1z -freflection %s

int main() {
  typename(reflexpr(int)) i = valueof(reflexpr(42));
  return 0;
}
