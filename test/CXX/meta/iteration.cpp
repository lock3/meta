// RUN: %clang_cc1 -std=c++1z -freflection %s

#include "reflection_iterator.h"

namespace N {
  void f1();
  void f2();
  void f3();
  void f4();
}

constexpr int count_members(meta::info x) {
  meta::iterator first = meta::begin(x);
  meta::iterator last = meta::end(x);
  int n = 0;
  while (first != last) {
    ++first;
    ++n;
  }
  return n;
}

int main(int argc, char* argv[]) {
  static_assert(count_members(reflexpr(N)) == 4);
}
