// RUN: %clang_cc1 -freflection -std=c++1z %s

#include "reflection_query.h"
#include "reflection_traits.h"

namespace A {
  inline namespace InlineA {
  }
}

namespace {
  namespace InnerA {
  }
}

int main() {
  // top level namespace traits
  {
    constexpr auto refl = reflexpr(A);
    constexpr auto traits = namespace_traits(__reflect(query_get_decl_traits, refl));

    static_assert(get_linkage(traits.linkage) == external_linkage);
    static_assert(get_access(traits.access) == no_access);
    static_assert(traits.is_inline == false);
  }

  // inline namespace traits
  {
    constexpr auto refl = reflexpr(A::InlineA);
    constexpr auto traits = namespace_traits(__reflect(query_get_decl_traits, refl));

    static_assert(get_linkage(traits.linkage) == external_linkage);
    static_assert(get_access(traits.access) == no_access);
    static_assert(traits.is_inline == true);
  }

  // inner namespace traits
  {
    constexpr auto refl = reflexpr(InnerA);
    constexpr auto traits = namespace_traits(__reflect(query_get_decl_traits, refl));

    static_assert(get_linkage(traits.linkage) == internal_linkage);
    static_assert(get_access(traits.access) == no_access);
    static_assert(traits.is_inline == false);
  }

  return 0;
}
