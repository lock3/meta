// RUN: %clang_cc1 -std=c++2a -stdlib=libc++ -freflection -fsyntax-only -verify %s
struct S { };

namespace N { }

int main() {
  static_assert(reflexpr(N) == reflexpr(N));
  static_assert(reflexpr(N) != reflexpr(S));
  static_assert(reflexpr(int) == reflexpr(int));
  static_assert(reflexpr(int) != reflexpr(const int));

  static_assert(reflexpr(int) < reflexpr(int)); // expected-error {{invalid operands to binary expression ('meta::info' and 'meta::info')}}
  static_assert(reflexpr(int) > reflexpr(int)); // expected-error {{invalid operands to binary expression ('meta::info' and 'meta::info')}}
  static_assert(reflexpr(int) <= reflexpr(int)); // expected-error {{invalid operands to binary expression ('meta::info' and 'meta::info')}}
  static_assert(reflexpr(int) >= reflexpr(int)); // expected-error {{invalid operands to binary expression ('meta::info' and 'meta::info')}}
  static_assert(reflexpr(int) <=> reflexpr(int)); // expected-error {{invalid operands to binary expression ('meta::info' and 'meta::info')}}
}
