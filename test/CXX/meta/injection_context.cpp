// RUN: %clang_cc1 -verify -freflection -std=c++1z %s

constexpr auto namespace_frag = __fragment namespace {
};

class clazz {
  consteval -> namespace_frag; // expected-error {{injecting namespace members into a class}}
};

constexpr auto class_frag = __fragment class {
};

namespace ns {
  consteval -> class_frag; // expected-error {{injecting class members into a namespace}}
}

