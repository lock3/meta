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

