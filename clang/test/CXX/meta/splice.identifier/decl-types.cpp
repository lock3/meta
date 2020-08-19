// RUN: %clang_cc1 -std=c++2a -freflection -verify %s
// expected-no-diagnostics

namespace enums {

enum unqualid("enum_t") { };

enum class unqualid("enum_class_t");
enum class unqualid("enum_class_t") { };

void test() {
  enum_t et;
  enum_class_t ect;
}

} // end namespace enums

namespace classes {

struct unqualid("struct_t");
struct unqualid("struct_t") { };

class unqualid("class_t");
class unqualid("class_t") { };

void test() {
  struct_t st;
  class_t ct;
}

} // end namespace classes

namespace namespaces {

namespace unqualid("namespace_ns") {
  int x;
}

namespace unqualid("namespace_ns")::unqualid("nested") {
  int x;
}

void test() {
  namespace_ns::x += 1;
  namespace_ns::nested::x += 1;
}

} // end namespace namespaces
