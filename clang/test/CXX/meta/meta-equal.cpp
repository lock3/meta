// RUN: %clang_cc1 -std=c++2a -stdlib=libc++ -freflection -fsyntax-only -verify %s
struct S { };
using AliasS = S;

namespace N { }

int main() {
  static_assert(^N == ^N);
  static_assert(^N != ^S);
  static_assert(^S == ^S);
  static_assert(^S == ^AliasS);
  static_assert(^int == ^int);
  static_assert(^int != ^const int);

  static_assert(^int < ^int); // expected-error {{invalid operands to binary expression ('meta::info' and 'meta::info')}}
  static_assert(^int > ^int); // expected-error {{invalid operands to binary expression ('meta::info' and 'meta::info')}}
  static_assert(^int <= ^int); // expected-error {{invalid operands to binary expression ('meta::info' and 'meta::info')}}
  static_assert(^int >= ^int); // expected-error {{invalid operands to binary expression ('meta::info' and 'meta::info')}}
  static_assert(^int <=> ^int); // expected-error {{invalid operands to binary expression ('meta::info' and 'meta::info')}}
}
