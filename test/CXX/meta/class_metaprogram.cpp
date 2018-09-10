// RUN: /Users/wyatt/Projects/llvm-build/bin/clang++ -std=c++1z -freflection /Users/wyatt/Projects/clang/test/CXX/meta/class_metaprogram.cpp

#include <experimental/meta>

constexpr auto fragment = __fragment struct {
  int frag_num() {
    return 2;
  }
};

class Foo {
  constexpr {
    -> fragment;
  }
};

int main() {
  Foo f;
  assert(f.frag_num() == 2);
  return 0;
};
