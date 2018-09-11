// RUN: /Users/wyatt/Projects/llvm-build/bin/clang++ -std=c++1z -freflection /Users/wyatt/Projects/clang/test/CXX/meta/class_metaprogram.cpp

#include <experimental/meta>

constexpr auto inner_fragment = __fragment struct {
  int inner_frag_num() {
    return 0;
  }
};

constexpr auto fragment = __fragment struct {
  constexpr {
    -> inner_fragment;
  }

  int x = 1;

  int frag_num() {
    return 2;
  }

  typedef int fragment_int;
};

class Foo {
  constexpr {
    -> fragment;
  }
};

int main() {
  Foo f;

  assert(f.x == 1);
  assert(f.frag_num() == 2);
  assert(f.inner_frag_num() == 0);

  Foo::fragment_int int_of_injected_type = 1;
  assert(static_cast<int>(int_of_injected_type) == 1);
  return 0;
};
