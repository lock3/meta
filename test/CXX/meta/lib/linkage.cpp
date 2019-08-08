// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

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
    static_assert(!__reflect(query_has_linkage, refl));
    static_assert(!__reflect(query_is_externally_linked, refl));
    static_assert(!__reflect(query_is_internally_linked, refl));
  }

  // static local var traits
  {
    static int static_local_var = 0;

    constexpr auto refl = reflexpr(static_local_var);
    static_assert(!__reflect(query_has_linkage, refl));
    static_assert(!__reflect(query_is_externally_linked, refl));
    static_assert(!__reflect(query_is_internally_linked, refl));
  }

  // global var traits
  {
    constexpr auto refl = reflexpr(global_var);
    static_assert(__reflect(query_has_linkage, refl));
    static_assert(__reflect(query_is_externally_linked, refl));
    static_assert(!__reflect(query_is_internally_linked, refl));
  }

  // static global var traits
  {
    constexpr auto refl = reflexpr(static_global_var);
    static_assert(__reflect(query_has_linkage, refl));
    static_assert(!__reflect(query_is_externally_linked, refl));
    static_assert(__reflect(query_is_internally_linked, refl));
  }

  // static inline global var traits
  {
    constexpr auto refl = reflexpr(static_inline_global_var);
    static_assert(__reflect(query_has_linkage, refl));
    static_assert(!__reflect(query_is_externally_linked, refl));
    static_assert(__reflect(query_is_internally_linked, refl));
  }

  // external global var traits
  {
    constexpr auto refl = reflexpr(external_global_var);
    static_assert(__reflect(query_has_linkage, refl));
    static_assert(__reflect(query_is_externally_linked, refl));
    static_assert(!__reflect(query_is_internally_linked, refl));
  }

  // thread local global var traits
  {
    constexpr auto refl = reflexpr(thread_local_global_var);
    static_assert(__reflect(query_has_linkage, refl));
    static_assert(__reflect(query_is_externally_linked, refl));
    static_assert(!__reflect(query_is_internally_linked, refl));
  }

  // static member data
  {
    constexpr auto refl = reflexpr(Class::public_access_static);
    static_assert(__reflect(query_has_linkage, refl));
    static_assert(__reflect(query_is_externally_linked, refl));
    static_assert(!__reflect(query_is_internally_linked, refl));
  }

  return 0;
}
