// RUN: %clang_cc1 -std=c++2a -freflection -verify %s
// expected-no-diagnostics

using info = decltype(reflexpr(void));

template<info T>
constexpr auto refl_add() {
  return 1 + typename [<T>](1);
}

auto y = refl_add<reflexpr(int)>();
