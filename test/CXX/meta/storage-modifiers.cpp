// RUN: %clang -freflection -std=c++2a %s

#include "reflection_query.h"
#include "reflection_mod.h"

namespace make_static_test {
  struct Existing {
    int field_1;
  };

  struct New {
    consteval {
      auto refl = reflexpr(Existing);

      // Fields
      auto field_1 = __reflect(query_get_begin, refl);
      __reflect_mod(query_set_storage, field_1, StorageModifier::Static);

      -> field_1;
    }
  };

  // Fields
  int New::field_1 = 0;
}

namespace make_static_aborted_test {
  struct Existing {
    int field_1;
  };

  struct New {
    consteval {
      auto refl = reflexpr(Existing);

      // Fields
      auto field_1 = __reflect(query_get_begin, refl);
      __reflect_mod(query_set_storage, field_1, StorageModifier::NotModified);

      -> field_1;
    }
  };

  void foo() {
    New n;

    // Fields
    n.field_1 = 0;
  }
}

int main() {
  return 0;
}
