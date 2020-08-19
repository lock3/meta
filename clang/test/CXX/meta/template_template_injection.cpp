// RUN: %clang_cc1 -freflection -std=c++2a %s

enum class enum_a { val_1, val_2 };
enum class enum_b { val_1, val_2 };

template<enum_a, enum_b>
class bar {
public:
  operator int() { return 0; }
};

template<template<enum_a, enum_b> typename C>
class foo {
  consteval -> fragment struct {
    int x = C<enum_a::val_1, enum_b::val_1>();

    void k(C<enum_a::val_1, enum_b::val_1> c) { }
  };
};

int main() {
  foo<bar> f;
  return 0;
}
