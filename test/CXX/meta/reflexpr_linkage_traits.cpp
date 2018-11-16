// RUN: %clang_cc1 -freflection -std=c++1z %s

#include "reflection_query.h"
#include "reflection_traits.h"

int global_var = 0;
static int static_global_var = 0;
static inline int static_inline_global_var = 0;
extern int external_global_var;
thread_local int thread_local_global_var = 0;

class Class {
public:
  const static int public_access_static = 0;
};

int main() {
  // local var traits
  {
    int local_var = 0;

    constexpr auto refl = reflexpr(local_var);
    constexpr auto traits = linkage_traits(__reflect(query_get_linkage_traits, refl));

    static_assert(traits.kind == no_linkage);
  }

  // static local var traits
  {
    static int static_local_var = 0;

    constexpr auto refl = reflexpr(static_local_var);
    constexpr auto traits = linkage_traits(__reflect(query_get_linkage_traits, refl));

    static_assert(traits.kind == no_linkage);
  }

  // global var traits
  {
    constexpr auto refl = reflexpr(global_var);
    constexpr auto traits = linkage_traits(__reflect(query_get_linkage_traits, refl));

    static_assert(traits.kind == external_linkage);
  }

  // static global var traits
  {
    constexpr auto refl = reflexpr(static_global_var);
    constexpr auto traits = linkage_traits(__reflect(query_get_linkage_traits, refl));

    static_assert(traits.kind == internal_linkage);
  }

  // static inline global var traits
  {
    constexpr auto refl = reflexpr(static_inline_global_var);
    constexpr auto traits = linkage_traits(__reflect(query_get_linkage_traits, refl));

    static_assert(traits.kind == internal_linkage);
  }

  // external global var traits
  {
    constexpr auto refl = reflexpr(external_global_var);
    constexpr auto traits = linkage_traits(__reflect(query_get_linkage_traits, refl));

    static_assert(traits.kind == external_linkage);
  }

  // thread local global var traits
  {
    constexpr auto refl = reflexpr(thread_local_global_var);
    constexpr auto traits = linkage_traits(__reflect(query_get_linkage_traits, refl));

    static_assert(traits.kind == external_linkage);
  }

  // static member data
  {
    constexpr auto refl = reflexpr(Class::public_access_static);
    constexpr auto traits = linkage_traits(__reflect(query_get_linkage_traits, refl));

    static_assert(traits.kind == external_linkage);
  }

  return 0;
}
