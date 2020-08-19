// RUN: %clang_cc1 -std=c++2a -freflection -fsyntax-only -verify %s

template<auto F>
constexpr auto test() { // expected-note {{candidate template ignored: invalid explicitly-specified argument for template parameter 'F'}}
  consteval -> F;
}

consteval auto get_frag() {
  int captured_value = 10;
  return fragment {
    return captured_value;
  };
}

int main() {
  constexpr int i = test<get_frag()>(); // expected-error {{no matching function for call to 'test'}}
  static_assert(i == 10);

  return 0;
}
