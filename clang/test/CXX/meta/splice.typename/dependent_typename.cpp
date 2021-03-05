// RUN: %clang_cc1 -std=c++2a -freflection -verify %s
// expected-no-diagnostics

template<typename T>
class s {
};

template<typename T, T V>
auto foo() {
  using type = typename [: V :];
  return type { };
}

using info = decltype(^void);

auto bar = foo<info, ^int>();
