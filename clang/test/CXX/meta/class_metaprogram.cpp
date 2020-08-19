// RUN: %clang_cc1 -freflection -std=c++2a %s

#define assert(E) if (!(E)) __builtin_abort();

const int global_int = 42;

struct InternalFragClass {
  static int instance_count;

  InternalFragClass() {
    instance_count += 1;
  }

  ~InternalFragClass() {
    instance_count -= 1;
  }
};

int InternalFragClass::instance_count = 0;

constexpr auto inner_frag = fragment struct S {
  int* c0 = new int(5);
  int* c1;

  S() : c1(new int(10)) { }
  ~S() {
    delete c0;
    delete c1;
  }

  int inner_frag_num() {
    return 0;
  }

  int inner_proxy_frag_num() {
    return this->y;
  }

  int referenced_global() {
    return %{global_int};
  }
};

constexpr auto frag = fragment struct X {
  consteval -> %{inner_frag};

  InternalFragClass FragClass;
  int x = 1;
  int z = this->y;

  template<typename T>
  int get_z(const T& t) {
    return t.z;
  }

  int frag_num() {
    return 2;
  }

  int proxy_frag_num() {
    return this->y;
  }

  typedef int frag_int;

  struct Nested {
    int bar;

    Nested(int bar) : bar(bar) { }

    int get_y(const X& t) {
      return t.y;
    }
  };

public:
  enum enum_a {
    EN_A = 1,
    EN_B = 3
  };

  enum class enum_class_a {
    EN_A = 5,
    EN_B = 10
  };
};

class Foo {
  int y = 55;

  consteval {
    -> frag;
  }

  consteval {
    int captured_int_val = 7;
    -> fragment struct K {
      int captured_int = %{captured_int_val};

      static constexpr int static_captured_int = %{captured_int_val};
      static_assert(static_captured_int == %{captured_int_val});
    };
  }

public:
  int dependent_on_injected_val() {
    return this->x;
  }
};

int main() {
  {
    Foo f;

    assert(f.x == 1);
    assert(f.dependent_on_injected_val() == 1);
    assert(f.frag_num() == 2);
    assert(f.inner_frag_num() == 0);
    assert(f.z == 55);
    assert(f.proxy_frag_num() == 55);
    assert(f.inner_proxy_frag_num() == 55);
    assert(f.referenced_global() == 42);
    assert(*f.c0 == 5);
    assert(*f.c1 == 10);
    assert(f.get_z(f) == 55);
    assert(f.captured_int == 7);

    Foo::frag_int int_of_injected_type = 1;
    assert(static_cast<int>(int_of_injected_type) == 1);

    assert(InternalFragClass::instance_count == 1);
  }

  {
    Foo f;
    Foo::Nested nested(5);

    assert(nested.bar == 5);
    assert(nested.get_y(f) == 55);
  }

  assert(InternalFragClass::instance_count == 0);
  assert(Foo::EN_A == 1);
  assert(Foo::EN_B == 3);
  assert(Foo::enum_class_a::EN_A == static_cast<Foo::enum_class_a>(5));
  assert(Foo::enum_class_a::EN_B == static_cast<Foo::enum_class_a>(10));

  return 0;
};
