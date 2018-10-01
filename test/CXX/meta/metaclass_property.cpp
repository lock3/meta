// RUN: %clangxx -std=c++1z -freflection %s

#include <experimental/meta>

template<typename T>
constexpr void property(T source) {
  -> __fragment struct X {
    int inner = 1;
  };
}

class X {
public:
  class(property) W {}; W width;
  class(property) L {}    length;
  class(property)   {}    height;
};

int main() {
  X x;
  assert(x.width.inner == 1);
  assert(x.length.inner == 1);
  assert(x.height.inner == 1);
}
