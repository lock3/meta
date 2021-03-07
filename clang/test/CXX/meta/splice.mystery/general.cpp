// RUN: %clang_cc1 -std=c++2a -freflection -verify %s
// expected-no-diagnostics

class X;
class Y;
class Z;

template<typename T>
X *foo() {
  return nullptr;
}

template<int z>
Y *foo() {
  return nullptr;
}

template<template<typename> typename C>
Z *foo() {
  return nullptr;
}

template<typename T>
struct tmpl_s {
};

namespace non_dependent {

X *foo_res_x = foo<[: ^int :]>();
Y *foo_res_y = foo<[: ^1 :]>();
Z *foo_res_z = foo<[: ^tmpl_s :]>();

}

namespace dependent {

using info = decltype(^void);

template<info InputVal>
auto test() {
  return foo<[: InputVal :]>();
}

X *foo_res_x = test<^int>();
Y *foo_res_y = test<^1>();
Z *foo_res_z = test<^tmpl_s>();

}
