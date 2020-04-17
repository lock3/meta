// RUN: %clang_cc1 -freflection -std=c++2a %s

namespace shape {
  namespace area { }

  struct square {
    unsigned width, height;

    consteval -> namespace(area) fragment namespace {
      requires typename square;

      constexpr unsigned calculate(const square& s) {
        return s.width * s.height;
      }
    };
  };
}

int main() {
  constexpr shape::square square { 10, 5 };
  static_assert(shape::area::calculate(square) == 50);
}
