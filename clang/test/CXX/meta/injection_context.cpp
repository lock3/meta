// RUN: %clang_cc1 -verify -freflection -std=c++2a %s

constexpr auto namespace_frag = fragment namespace { };
constexpr auto class_frag = fragment class { };
constexpr auto member_block_frag = fragment this { };
constexpr auto enum_frag = fragment enum { };
constexpr auto block_frag = fragment { };

namespace ns {
  consteval -> namespace_frag;
  consteval -> class_frag; // expected-error {{injecting class members into a namespace}}
  consteval -> member_block_frag; // expected-error {{injecting member statements into a namespace}}
  consteval -> enum_frag; // expected-error {{injecting enum members into a namespace}}
  consteval -> block_frag; // expected-error {{injecting statements into a namespace}}
}

class clazz {
  consteval -> namespace_frag; // expected-error {{injecting namespace members into a class}}
  consteval -> class_frag;
  consteval -> member_block_frag; // expected-error {{injecting member statements into a class}}
  consteval -> enum_frag; // expected-error {{injecting enum members into a class}}
  consteval -> block_frag; // expected-error {{injecting statements into a class}}

  void func() {
    consteval -> namespace_frag; // expected-error {{injecting namespace members into a member function}}
    consteval -> class_frag; // expected-error {{injecting class members into a member function}}
    consteval -> member_block_frag;
    consteval -> enum_frag; // expected-error {{injecting enum members into a member function}}
    consteval -> block_frag; // expected-error {{injecting statements into a member function}}
  }
};

enum enam {
  consteval -> namespace_frag, // expected-error {{injecting namespace members into an enum}}
  consteval -> class_frag, // expected-error {{injecting class members into an enum}}
  consteval -> member_block_frag // expected-error {{injecting member statements into an enum}}
  consteval -> enum_frag,
  consteval -> block_frag // expected-error {{injecting statements into an enum}}
};

void func() {
  consteval -> namespace_frag; // expected-error {{injecting namespace members into a free function}}
  consteval -> class_frag; // expected-error {{injecting class members into a free function}}
  consteval -> member_block_frag; // expected-error {{injecting member statements into a free function}}
  consteval -> enum_frag; // expected-error {{injecting enum members into a free function}}
  consteval -> block_frag;
}
