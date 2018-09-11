// RUN: %clang_cc1 -I%S/usr/include -I%S/usr/local/include/c++/v1 -fsyntax-only -verify -std=c++1z -freflection %s

#include <experimental/meta>

namespace class_default_tests {
  constexpr auto frag = __fragment class {
    int i = 0; // expected-note {{implicitly declared private here}}

    void bar() { // expected-note {{implicitly declared private here}}
    }
  };

  class Foo {
    constexpr {
      -> frag;
    }
  };

  void run() {
    Foo f;
    f.i = 3; // expected-error {{'i' is a private member of 'class_default_tests::Foo'}}
    f.bar(); // expected-error {{'bar' is a private member of 'class_default_tests::Foo'}}
  }
}

namespace struct_default_tests {
  constexpr auto frag = __fragment struct {
    int i = 0;

    void bar() {
    }
  };

  class Foo {
    constexpr {
      -> frag;
    }
  };

  void run() {
    Foo f;
    f.i = 3;
    f.bar();
  }
}

namespace class_modified_tests {
  constexpr auto frag = __fragment class {
  public:
    int i = 0;

    void bar() {
    }
  };

  class Foo {
    constexpr {
      -> frag;
    }
  };

  void run() {
    Foo f;
    f.i = 3;
    f.bar();
  }
}

namespace struct_modified_tests {
  constexpr auto frag = __fragment struct {
  private:
    int i = 0; // expected-note {{implicitly declared private here}}

    void bar() { // expected-note {{implicitly declared private here}}
    }
  };

  class Foo {
    constexpr {
      -> frag;
    }
  };

  void run() {
    Foo f;
    f.i = 3; // expected-error {{'i' is a private member of 'struct_modified_tests::Foo'}}
    f.bar(); // expected-error {{'bar' is a private member of 'struct_modified_tests::Foo'}}
  }
}
