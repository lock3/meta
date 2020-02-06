// RUN: %clang_cc1 -freflection -std=c++2a %s

constexpr auto foo = __fragment this {
  return this->bar;
};

class produced {
  int bar;

public:
  constexpr produced(int bar)
    : bar(bar) { }

  constexpr int get_bar() const {
    consteval -> foo;
  }
};

constexpr produced val(10);
static_assert(val.get_bar() == 10);
