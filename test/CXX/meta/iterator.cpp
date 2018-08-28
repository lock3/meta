// RUN: %clang_cc1 -I%S/usr/include -I%S/usr/local/include/c++/v1 -std=c++1z -freflection %s

#include <experimental/meta>
// #include <cstdio>
// #include <cppx/compiler>

// #include <iostream>
// #include <typeinfo>

using namespace std::experimental;

namespace N {
  void f1();
  void f2();
  void f3();
  void f4();
}

constexpr int count_members(meta::info x) {
  meta::iterator first = begin(x);
  meta::iterator last = end(x);
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
