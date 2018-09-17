// RUN: %clangxx -std=c++1z -freflection %s

#include <experimental/meta>

constexpr auto fragment = __fragment namespace {
  int v_one = 1;
  int v_two = 2;

  int add_v_one(int num) {
    return v_one + num;
  }
};

namespace foo {
  constexpr {
    -> fragment;
  }
};

int main() {
  assert(foo::v_one == 1);
  assert(foo::v_two == 2);
  assert(foo::add_v_one(1) == 2);

  return 0;
};
