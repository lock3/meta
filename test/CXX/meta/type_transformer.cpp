// RUN: %clang_cc1 -freflection -std=c++1z %s

#include "reflection_query.h"
#include "reflection_iterator.h"

constexpr void dupe(meta::info source) {
  for (meta::info mem : meta::range(source)) {
    -> mem;
  };
};

struct OldType {
  static constexpr int x = 10;
  static constexpr int y = 20;
};

using struct NewType as dupe(reflexpr(OldType));

int main() {
  static_assert(NewType::x == 10);
  static_assert(NewType::y == 20);

  return 0;
}
