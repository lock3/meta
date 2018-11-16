// RUN: %clang_cc1 -freflection -std=c++1z %s

#include "reflection_query.h"
#include "reflection_traits.h"

class Class {
public:
  const static int public_access_static = 0;
};

int main() {
    {
      int local_var = 0;

      constexpr auto refl = reflexpr(local_var);
      constexpr auto traits = access_traits(__reflect(query_get_access_traits, refl));

      static_assert(traits.kind == no_access);
    }

    {
      constexpr auto refl = reflexpr(Class::public_access_static);
      constexpr auto traits = access_traits(__reflect(query_get_access_traits, refl));

      static_assert(traits.kind == public_access);
    }

  return 0;
}
