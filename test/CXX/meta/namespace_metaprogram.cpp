// RUN: %clangxx -std=c++1z -freflection %s

#include <experimental/meta>

constexpr auto fragment = __fragment namespace {
  int x = 1;
};

namespace Foo {
  constexpr {
    -> fragment;
  }
};

int main() {
  assert(Foo::x == 1);

  return 0;
};
