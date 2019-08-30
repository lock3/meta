// RUN: %clang_cc1 -freflection -std=c++1z %s

template<typename T>
constexpr void property(T source) {
  -> __fragment struct X {
    int inner = 1;
    constexpr X() = default;
  };
}

class X {
public:
  constexpr X() = default;
  class(property) W {}; W width;
  class(property) L {}    length;
  class(property)   {}    height;
};

int main() {
  constexpr X x;
  static_assert(x.width.inner == 1);
  static_assert(x.length.inner == 1);
  static_assert(x.height.inner == 1);
}
