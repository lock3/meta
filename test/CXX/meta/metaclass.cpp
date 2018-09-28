// RUN: %clangxx -std=c++1z -freflection %s

#include <experimental/meta>

template<typename T>
constexpr void test(T source) {
  auto frag =  __fragment struct {
    int h_1() {
      return 1;
    }
  };

  -> frag;
}

class(test) Test {
};

int main() {
  Test t;
  assert(t.h_1() == 1);
  return 0;
}
