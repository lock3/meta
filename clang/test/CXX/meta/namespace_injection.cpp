// RUN: %clang_cc1 -std=c++2a -freflection %s

namespace basic_ns_inject {

consteval {
  -> fragment namespace {
    namespace {
      void foo() { }
    }
  };
}

void test() {
  foo();
}

} // end namespace basic_ns_inject


namespace dependent_ns_inject {

template<int I>
class foo {
  consteval {
    -> namespace fragment namespace {
      namespace {
        void unqualid("bar", I)() { }
      }
    };
  }
};

void test1() {
  foo<1> x;
  bar1();
}

} // end namespace dependent_ns_inject
