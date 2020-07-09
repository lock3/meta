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

template<int Y>
void templ_test() {
  int q = typename unqualid("S", Y)<2>::X2();
}

void test() {
  templ_test<1>();
}

} // end namespace dependent_class_name

namespace dependent_namespace_name {

namespace S1 {
  class C {
  };
}
template<int Y>
void templ_test() {
  auto c = unqualid("S", Y)::C();
}

void test() {
  templ_test<1>();
}

} // end namespace dependent_class_name

