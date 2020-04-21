// RUN: %clang_cc1 -freflection -std=c++2a -fsyntax-only -verify %s

constexpr auto frag = fragment {
  return 0;
};

int main() {
  consteval -> frag(); // expected-error {{called object type 'meta::info' is not a function or function pointer}}
  return 0;
}
