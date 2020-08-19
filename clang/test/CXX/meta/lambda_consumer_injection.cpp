// RUN: %clang -freflection -std=c++2a %s

template<typename F>
consteval void lambda_consumer(F op) {
  op(1);
}

enum class type { a, b, c };

template<template<type> class E>
class foo {
  consteval {
    lambda_consumer([](auto val) consteval {
      -> fragment struct {
        int get(E<type::a> arg) {
          return %{val};
        }
      };
    });
  }
};

template<type T>
struct bar {
};

int main() {
  foo<bar> f;
  return f.get({});
}
