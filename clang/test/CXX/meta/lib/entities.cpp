// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../support/query.h"

namespace get_template {
  namespace clazz {
    namespace refl {
      template<typename A, int B>
      class Foo { };

      template<typename A, int B>
      class Bar { };
    }

    constexpr auto base_template_refl = ^refl::Foo;

    namespace is {
      constexpr auto specialization_refl = ^refl::Foo<int, 1>;

      static_assert(base_template_refl == __reflect(query_get_template, specialization_refl));
    }

    namespace is_not {
      constexpr auto specialization_refl = ^refl::Bar<int, 1>;

      static_assert(base_template_refl != __reflect(query_get_template, specialization_refl));
    }
  }
}

