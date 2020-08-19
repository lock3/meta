// RUN: %clangxx -std=c++2a -freflection -Wno-deprecated-fragment %s

#define assert(E) if (!(E)) __builtin_abort();

constexpr auto frag = __fragment namespace {
  int v_one = 1;
  int v_two = 2;

  int add_v_one(int num) {
    return v_one + num;
  }

  template<typename T>
  T add_v_two(T num) {
    return v_two + num;
  }

  namespace bar {
    int v_eleven = 11;
  }

  struct class_a {
    int x = 1;
  };

  struct class_b {
    int x = 1;
  };

  enum enum_a {
    EN_A = 1,
    EN_B = 3
  };

  enum class enum_class_a {
    EN_A = 5,
    EN_B = 10
  };
};

namespace foo {
  consteval {
    -> frag;
  }
};

int main() {
  assert(foo::v_one == 1);
  assert(foo::v_two == 2);
  assert(foo::add_v_one(1) == 2);
  assert(foo::add_v_two(1) == 3);

  assert(foo::bar::v_eleven == 11);

  {
    foo::class_a clazz;
    assert(clazz.x == 1);
  }

  {
    foo::class_b clazz;
    assert(clazz.x == 1);
  }

  assert(foo::EN_A == 1);
  assert(foo::EN_B == 3);

  assert(foo::enum_class_a::EN_A == static_cast<foo::enum_class_a>(5));
  assert(foo::enum_class_a::EN_B == static_cast<foo::enum_class_a>(10));

  return 0;
};
