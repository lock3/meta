// RUN: %clang_cc1 -freflection -std=c++2a %s

constexpr auto modify_bar = fragment this {
  int modification = 5;
  this->bar += modification;
};
constexpr auto return_bar = fragment this {
  return this->bar;
};

class produced {
  int bar;

public:
  constexpr produced(int bar)
    : bar(bar) {
    consteval -> modify_bar;
  }

  constexpr int get_bar() const {
    consteval -> return_bar;
  }
};

constexpr produced val(5);
static_assert(val.get_bar() == 10);

template<typename T>
class produced_template {
  T bar;

public:
  constexpr produced_template(T bar)
    : bar(bar) {
    consteval -> modify_bar;
  }

  constexpr T get_bar() const {
    consteval -> return_bar;
  }
};

constexpr produced_template<int> templ_val(5);
static_assert(templ_val.get_bar() == 10);
