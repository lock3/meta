// RUN: %clang_cc1 -freflection -std=c++1z %s

#include "reflection_query.h"
#include "reflection_traits.h"

int global_var = 0;
static int static_global_var = 0;
static inline int static_inline_global_var = 0;
extern int external_global_var;

class Class {
  const static int default_access_static = 0;
public:
  const static int public_access_static = 0;
protected:
  const static int protected_access_static = 0;
private:
  const static int private_access_static = 0;
};

int main() {
  // local var traits
  {
    int local_var = 0;

    constexpr auto refl = reflexpr(local_var);
    constexpr auto traits = variable_traits(__reflect(query_get_decl_traits, refl));

    static_assert(get_linkage(traits.linkage) == no_linkage);
    // static_assert(get_storage(traits.storage) == no_storage);
    static_assert(traits.is_defined == true);
    static_assert(traits.is_inline == false);
  }

  // static local var traits
  {
    static int static_local_var = 0;

    constexpr auto refl = reflexpr(static_local_var);
    constexpr auto traits = variable_traits(__reflect(query_get_decl_traits, refl));

    static_assert(get_linkage(traits.linkage) == no_linkage);
    // static_assert(get_storage(traits.storage) == no_storage);
    static_assert(traits.is_defined == true);
    static_assert(traits.is_inline == false);
  }

  // global var traits
  {
    constexpr auto refl = reflexpr(global_var);
    constexpr auto traits = variable_traits(__reflect(query_get_decl_traits, refl));

    static_assert(get_linkage(traits.linkage) == external_linkage);
    // static_assert(get_storage(traits.storage) == no_storage);
    static_assert(traits.is_defined == true);
    static_assert(traits.is_inline == false);
  }

  // static global var traits
  {
    constexpr auto refl = reflexpr(static_global_var);
    constexpr auto traits = variable_traits(__reflect(query_get_decl_traits, refl));

    static_assert(get_linkage(traits.linkage) == internal_linkage);
    // static_assert(get_storage(traits.storage) == no_storage);
    static_assert(traits.is_defined == true);
    static_assert(traits.is_inline == false);
  }

  // static inline global var traits
  {
    constexpr auto refl = reflexpr(static_inline_global_var);
    constexpr auto traits = variable_traits(__reflect(query_get_decl_traits, refl));

    static_assert(get_linkage(traits.linkage) == internal_linkage);
    // static_assert(get_storage(traits.storage) == no_storage);
    static_assert(traits.is_defined == true);
    static_assert(traits.is_inline == true);
  }

  // external global var traits
  {
    constexpr auto refl = reflexpr(external_global_var);
    constexpr auto traits = variable_traits(__reflect(query_get_decl_traits, refl));

    static_assert(get_linkage(traits.linkage) == external_linkage);
    // static_assert(get_storage(traits.storage) == no_storage);
    static_assert(traits.is_defined == false);
    static_assert(traits.is_inline == false);
  }

  // static member data
  {
    constexpr auto refl = reflexpr(Class::public_access_static);
    constexpr auto traits = variable_traits(__reflect(query_get_decl_traits, refl));

    static_assert(get_linkage(traits.linkage) == external_linkage);
    // static_assert(get_storage(traits.storage) == no_storage);
    static_assert(traits.is_defined == false);
    static_assert(traits.is_inline == false);
  }

  // constexpr
  {
    {
      constexpr int local_var = 0;

      constexpr auto refl = reflexpr(local_var);
      constexpr auto traits = variable_traits(__reflect(query_get_decl_traits, refl));

      static_assert(traits.is_constexpr == true);
    }
    {
      int local_var = 0;

      constexpr auto refl = reflexpr(local_var);
      constexpr auto traits = variable_traits(__reflect(query_get_decl_traits, refl));

      static_assert(traits.is_constexpr == false);
    }
  }

  // var access
  {
    {
      int local_var = 0;

      constexpr auto refl = reflexpr(local_var);
      constexpr auto traits = variable_traits(__reflect(query_get_decl_traits, refl));

      static_assert(get_access(traits.access) == no_access);
    }

    // {
    //   constexpr auto refl = reflexpr(Class::default_access_static);
    //   constexpr auto traits = variable_traits(__reflect(query_get_decl_traits, refl));

    //   static_assert(get_access(traits.access) == private_access);
    // }

    // {
    //   constexpr auto refl = reflexpr(Class::public_access_static);
    //   constexpr auto traits = variable_traits(__reflect(query_get_decl_traits, refl));

    //   static_assert(get_access(traits.access) == public_access);
    // }

    // {
    //   constexpr auto refl = reflexpr(Class::protected_access_static);
    //   constexpr auto traits = variable_traits(__reflect(query_get_decl_traits, refl));

    //   static_assert(get_access(traits.access) == protected_access);
    // }

    // {
    //   constexpr auto refl = reflexpr(Class::private_access_static);
    //   constexpr auto traits = variable_traits(__reflect(query_get_decl_traits, refl));

    //   static_assert(get_access(traits.access) == private_access);
    // }
  }

  return 0;
}
