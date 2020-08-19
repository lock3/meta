// RUN: %clang_cc1 -freflection -std=c++2a %s

constexpr auto frag_d_e = fragment enum { D, E };
constexpr auto frag_f = fragment enum { F = 42 };
constexpr auto frag_h = fragment enum { H };

struct NonTemplated {
  enum Foo {
    A, B, C = 4,
    consteval {
      -> frag_d_e;
      -> frag_f;
    },
    consteval -> fragment enum { G },
    consteval -> frag_h
  };
};

template<int K>
struct Templated {
  enum Foo {
    A, B, C = 4,
    consteval {
      -> frag_d_e;
      -> frag_f;
    },
    consteval -> fragment enum { G },
    consteval -> frag_h
  };
};

struct NonTemplatedGen {
  enum Foo {
    consteval {
      for (int i = 0; i < 3; ++i) {
        -> fragment enum {
          unqualid("VAL_", %{i}) = %{i * 5}
        };
      }
    }
  };
};

template<int K>
struct TemplatedGen {
  enum Foo {
    consteval {
      for (int i = 0; i < 3; ++i) {
        -> fragment enum {
          unqualid("VAL_", %{i + K}) = %{i * 5}
        };
      }
    }
  };
};

int main() {
  {
    static_assert(NonTemplated::Foo::A == 0);
    static_assert(NonTemplated::Foo::B == 1);
    static_assert(NonTemplated::Foo::C == 4);
    static_assert(NonTemplated::Foo::D == 5);
    static_assert(NonTemplated::Foo::E == 6);
    static_assert(NonTemplated::Foo::F == 42);
    static_assert(NonTemplated::Foo::G == 43);
    static_assert(NonTemplated::Foo::H == 44);
  }
  {
    static_assert(Templated<1>::Foo::A == 0);
    static_assert(Templated<1>::Foo::B == 1);
    static_assert(Templated<1>::Foo::C == 4);
    static_assert(Templated<1>::Foo::D == 5);
    static_assert(Templated<1>::Foo::E == 6);
    static_assert(Templated<1>::Foo::F == 42);
    static_assert(Templated<1>::Foo::G == 43);
    static_assert(Templated<1>::Foo::H == 44);
  }
  {
    static_assert(NonTemplatedGen::Foo::VAL_0 == 0);
    static_assert(NonTemplatedGen::Foo::VAL_1 == 5);
    static_assert(NonTemplatedGen::Foo::VAL_2 == 10);
  }
  {
    static_assert(TemplatedGen<1>::Foo::VAL_1 == 0);
    static_assert(TemplatedGen<1>::Foo::VAL_2 == 5);
    static_assert(TemplatedGen<1>::Foo::VAL_3 == 10);
  }
  return 0;
}
