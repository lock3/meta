// RUN: %clangxx -std=c++1z -freflection %s

#include <experimental/meta>

int global_int = 42;

constexpr auto inner_fragment = __fragment struct S {
  int* c0 = new int(5);
  int* c1;

  S() : c1(new int(10)) { }
  ~S() {
    delete c0;
    delete c1;
  }

  int inner_frag_num() {
    return 0;
  }

  int inner_proxy_frag_num() {
    return this->y;
  }

  int referenced_global() {
    return global_int;
  }
};

constexpr auto fragment = __fragment struct {
  constexpr {
    -> inner_fragment;
  }

  int x = 1;
  int z = this->y;

  int frag_num() {
    return 2;
  }

  int proxy_frag_num() {
    return this->y;
  }

  typedef int fragment_int;
};

class Foo {
  int y = 55;

  constexpr {
    -> fragment;
  }

public:
  int dependent_on_injected_val() {
    return this->x;
  }
};

int main() {
  Foo f;

  assert(f.x == 1);
  assert(f.dependent_on_injected_val() == 1);
  assert(f.frag_num() == 2);
  assert(f.inner_frag_num() == 0);
  assert(f.z == 55);
  assert(f.proxy_frag_num() == 55);
  assert(f.inner_proxy_frag_num() == 55);
  assert(f.referenced_global() == 42);
  assert(*f.c0 == 5);
  assert(*f.c1 == 10);

  Foo::fragment_int int_of_injected_type = 1;
  assert(static_cast<int>(int_of_injected_type) == 1);
  return 0;
};
