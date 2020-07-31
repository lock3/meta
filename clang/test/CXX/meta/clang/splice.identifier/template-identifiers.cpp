// RUN: %clang_cc1 -std=c++2a -freflection -verify %s
// expected-no-diagnostics

namespace class_name {

template<int N>
struct S {
  static int X2 () {
    return N;
  }
};

void test() {
  int q = unqualid("S")<2>::X2();
}

} // end namespace class_name

namespace dependent_class_name {

template<int N>
struct S1 {
  static int X2 () {
    return N;
  }
};

template<int N>
void foo() {
  int q = typename unqualid("S", N)<2>::X2();
}

void test() {
  foo<1>();
}

} // end namespace dependent_class_name
