// RUN: %clang_cc1 -std=c++2a -freflection -verify %s
// expected-no-diagnostics

namespace local_var {

int foo() {
  int unqualid("x", 1) = { };
  int unqualid("x", 2) = { };
  return x1 + x2;
}

void test() {
  foo();
}

} // end namespace local_var

namespace dependent_local_var {

template<int T>
int foo() {
  int unqualid("x", T) = { };
  int unqualid("x", T + 1) = { };
  return unqualid("x", T) + unqualid("x", T + 1);
}

void test() {
  foo<1>();
}

} // end namespace dependent_local_var
