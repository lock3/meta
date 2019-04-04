// RUN: %clang_cc1 -verify -freflection -std=c++1z %s

constexpr auto namespace_frag = __fragment namespace { };
constexpr auto class_frag = __fragment class { };
constexpr auto enum_frag = __fragment enum { };

namespace ns {
  consteval -> namespace_frag;
  consteval -> class_frag; // expected-error {{injecting class members into a namespace}}
  consteval -> enum_frag; // expected-error {{injecting enum members into a namespace}}
}

class clazz {
  consteval -> namespace_frag; // expected-error {{injecting namespace members into a class}}
  consteval -> class_frag;
  consteval -> enum_frag; // expected-error {{injecting enum members into a class}}
};

enum enam {
  consteval -> namespace_frag, // expected-error {{injecting namespace members into an enum}}
  consteval -> class_frag, // expected-error {{injecting class members into an enum}}
  consteval -> enum_frag
};

