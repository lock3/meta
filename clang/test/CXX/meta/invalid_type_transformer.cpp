// RUN: %clang_cc1 -freflection -std=c++2a -fsyntax-only -verify %s

#include "reflection_query.h"
#include "reflection_iterator.h"

consteval void dupe(meta::info source) {
  for (meta::info mem : meta::range(source)) {
    -> mem;
  };
};

using struct NewType as dupe(10); // expected-error {{invalid operand of type 'int'; expected a reflection}}

int main() {
  static_assert(NewType::x == 10);
  static_assert(NewType::y == 20);

  return 0;
}
