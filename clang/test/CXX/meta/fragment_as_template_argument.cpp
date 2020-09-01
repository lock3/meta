// RUN: %clang_cc1 -std=c++2a -freflection -fsyntax-only -verify %s
// expected-no-diagnostics

template<auto F>
constexpr auto test() {
  consteval -> F;
}

consteval auto get_frag() {
  int captured_value = 10;
  return fragment {
    return %{captured_value};
  };
}

int main() {
  constexpr int i = test<get_frag()>();
  static_assert(i == 10);

  return 0;
}
