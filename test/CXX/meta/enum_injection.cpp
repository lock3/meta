// RUN: %clang_cc1 -freflection -std=c++1z %s

constexpr auto frag_d_e = __fragment enum { D, E };
constexpr auto frag_f = __fragment enum { F = 42 };
constexpr auto frag_h = __fragment enum { H };

struct NonTemplated {
  enum Foo {
    A, B, C = 4,
    consteval {
      -> frag_d_e;
      -> frag_f;
    },
    consteval -> __fragment enum { G },
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
    consteval -> __fragment enum { G },
    consteval -> frag_h
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
  return 0;
}
