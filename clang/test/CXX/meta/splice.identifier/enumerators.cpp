// RUN: %clang_cc1 -std=c++2a -freflection -verify %s
// expected-no-diagnostics

namespace enum_enumerators {

enum enum_n {
  [# "A", 1 #],
  [# "A", 2 #],
  [# "A", 3 #]
};

void test() {
  int x1 = A1 + A2 + A3;
}

} // end namespace enum_enumerators

namespace enum_class_enumerators {

enum class enum_n {
  [# "A", 1 #],
  [# "A", 2 #],
  [# "A", 3 #]
};

void test() {
  enum_n x1 = enum_n::A1;
  enum_n x2 = enum_n::A2;
  enum_n x3 = enum_n::A3;
}

} // end namespace enum_class_enumerators

