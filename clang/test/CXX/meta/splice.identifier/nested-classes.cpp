// RUN: %clang_cc1 -std=c++2a -freflection -verify %s
// expected-no-diagnostics

namespace nested_classes {

class container_type {
public:
  class unqualid("contained_type_", 1) {
    public:
    class unqualid("contained_contained_type_", 1) {
    };
  };
};

void test() {
  container_type container;
  container_type::contained_type_1 contained_container;
  container_type::contained_type_1::contained_contained_type_1 contained_contained_container;
}

} // end namespace inline_members

namespace dependent_nested_classes {

template<int T>
class container_type {
public:
  class unqualid("contained_type_", T) {
    public:
    class unqualid("contained_contained_type_", T) {
    };
  };
};

void test() {
  container_type<1> container;
  container_type<1>::contained_type_1 contained_container;
  container_type<1>::contained_type_1::contained_contained_type_1 contained_contained_container;
}

} // end namespace dependent_nested_classes

