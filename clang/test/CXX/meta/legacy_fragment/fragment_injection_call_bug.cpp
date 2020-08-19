// RUN: %clang_cc1 -freflection -Wno-deprecated-fragment -std=c++2a -fsyntax-only -verify %s

constexpr auto frag = __fragment {
  return 0;
};

int main() {
  consteval -> frag(); // expected-error {{called object type 'meta::info' is not a function or function pointer}}
  return 0;
}
