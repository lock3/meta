// RUN: %clang_cc1 -std=c++2a -freflection %s

namespace basic_alias_inject {

namespace xyz {
  class bar { };
}

consteval {
  -> fragment namespace {
    using namespace xyz;
  };
}

void test() {
  bar b;
}

} // end namespace basic_alias_inject


namespace dependent_alias_inject {

namespace xyz {
  class bar { };
}

template<int I>
class foo {
  consteval {
    -> namespace fragment namespace {
      using namespace xyz;
    };
  }
};

void test1() {
  foo<1> x;
  bar b;
}

} // end namespace dependent_alias_inject
