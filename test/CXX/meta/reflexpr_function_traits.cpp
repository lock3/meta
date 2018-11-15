// RUN: %clang_cc1 -freflection -std=c++1z %s

#include "reflection_query.h"
#include "reflection_traits.h"

void global_function() { }
void deleted_global_function() = delete;
void noexcept_global_function() noexcept { };
constexpr void constexpr_global_function() { }
static void static_global_function() { }
inline void inline_global_function() { }
static inline void static_inline_global_function() { }
extern void external_global_function();

class Class {
  const static void default_access_static() { }
public:
  const static void public_access_static() { }
protected:
  const static void protected_access_static() { }
private:
  const static void private_access_static() { }
};

int main() {
  // global function traits
  {
    constexpr auto refl = reflexpr(global_function);
    constexpr auto traits = function_traits(__reflect(query_get_decl_traits, refl));

    static_assert(get_linkage(traits.linkage) == external_linkage);
    // static_assert(get_storage(traits.storage) == no_storage);
    static_assert(traits.is_defined == true);
    static_assert(traits.is_inline == false);
  }

  // global deleted function traits
  // {
  //   constexpr auto refl = reflexpr(deleted_global_function);
  //   constexpr auto traits = function_traits(__reflect(query_get_decl_traits, refl));

  //   static_assert(get_linkage(traits.linkage) == external_linkage);
  //   // static_assert(get_storage(traits.storage) == no_storage);
  //   static_assert(traits.is_defined == true);
  //   static_assert(traits.is_deleted == true);
  // }

  // global noexcept function traits
  {
    constexpr auto refl = reflexpr(noexcept_global_function);
    constexpr auto traits = function_traits(__reflect(query_get_decl_traits, refl));

    static_assert(get_linkage(traits.linkage) == external_linkage);
    // static_assert(get_storage(traits.storage) == no_storage);
    static_assert(traits.is_defined == true);
    static_assert(traits.is_noexcept == true);
  }

  // global constexpr function traits
  {
    constexpr auto refl = reflexpr(constexpr_global_function);
    constexpr auto traits = function_traits(__reflect(query_get_decl_traits, refl));

    static_assert(get_linkage(traits.linkage) == external_linkage);
    // static_assert(get_storage(traits.storage) == no_storage);
    static_assert(traits.is_defined == true);
    static_assert(traits.is_constexpr == true);
  }

  // static global function traits
  {
    constexpr auto refl = reflexpr(static_global_function);
    constexpr auto traits = function_traits(__reflect(query_get_decl_traits, refl));

    static_assert(get_linkage(traits.linkage) == internal_linkage);
    // static_assert(get_storage(traits.storage) == no_storage);
    static_assert(traits.is_defined == true);
    static_assert(traits.is_inline == false);
  }

  // inline global function traits
  {
    constexpr auto refl = reflexpr(inline_global_function);
    constexpr auto traits = function_traits(__reflect(query_get_decl_traits, refl));

    static_assert(get_linkage(traits.linkage) == external_linkage);
    // static_assert(get_storage(traits.storage) == no_storage);
    static_assert(traits.is_defined == true);
    static_assert(traits.is_inline == true);
  }

  // static inline global function traits
  {
    constexpr auto refl = reflexpr(static_inline_global_function);
    constexpr auto traits = function_traits(__reflect(query_get_decl_traits, refl));

    static_assert(get_linkage(traits.linkage) == internal_linkage);
    // static_assert(get_storage(traits.storage) == no_storage);
    static_assert(traits.is_defined == true);
    static_assert(traits.is_inline == true);
  }

  // external global function traits
  {
    constexpr auto refl = reflexpr(external_global_function);
    constexpr auto traits = function_traits(__reflect(query_get_decl_traits, refl));

    static_assert(get_linkage(traits.linkage) == external_linkage);
    // static_assert(get_storage(traits.storage) == no_storage);
    static_assert(traits.is_defined == false);
    static_assert(traits.is_inline == false);
  }

  // static member function
  {
    constexpr auto refl = reflexpr(Class::public_access_static);
    constexpr auto traits = function_traits(__reflect(query_get_decl_traits, refl));

    static_assert(get_linkage(traits.linkage) == external_linkage);
    // static_assert(get_storage(traits.storage) == no_storage);
    static_assert(traits.is_defined == false);
    static_assert(traits.is_inline == false);
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
