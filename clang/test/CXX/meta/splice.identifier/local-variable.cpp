// RUN: %clang_cc1 -std=c++2a -freflection -verify %s
// expected-no-diagnostics

namespace local_var {

int foo() {
  int [# "x", 1 #] = { };
  int [# "x", 2 #] = { };
  return x1 + x2;
}

void test() {
  foo();
}

} // end namespace local_var

namespace dependent_local_var {

template<int T>
int foo() {
  int [# "x", T #] = { };
  int [# "x", T + 1 #] = { };
  return [# "x", T #] + [# "x", T + 1 #];
}

void test() {
  foo<1>();
}

} // end namespace dependent_local_var
