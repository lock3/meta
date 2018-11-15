// RUN: %clang_cc1 -freflection -std=c++1z %s

#include "reflection_query.h"
#include "reflection_traits.h"

class Class {
  const int default_access = 0;
public:
  int data_member = 0;
  mutable int mutable_data_member = 0;
  const int const_data_member = 0;
  const int public_access = 0;
protected:
  const int protected_access = 0;
private:
  const int private_access = 0;
};

namespace {

class InternalClass {
public:
  int data_member = 0;
};

}

int main() {
  // data member of exposed class traits
  {
    constexpr auto refl = reflexpr(Class::data_member);
    constexpr auto traits = field_traits(__reflect(query_get_decl_traits, refl));

    static_assert(get_linkage(traits.linkage) == external_linkage);
    // static_assert(get_storage(traits.storage) == no_storage);
  }

  // data member of internal class traits
  {
    constexpr auto refl = reflexpr(InternalClass::data_member);
    constexpr auto traits = field_traits(__reflect(query_get_decl_traits, refl));

    static_assert(get_linkage(traits.linkage) == internal_linkage);
    // static_assert(get_storage(traits.storage) == no_storage);
  }

  // mutablability
  {
    {
      constexpr auto refl = reflexpr(Class::data_member);
      constexpr auto traits = field_traits(__reflect(query_get_decl_traits, refl));

      static_assert(traits.is_mutable == false);
    }

    {
      constexpr auto refl = reflexpr(Class::mutable_data_member);
      constexpr auto traits = field_traits(__reflect(query_get_decl_traits, refl));

      static_assert(traits.is_mutable == true);
    }

    {
      constexpr auto refl = reflexpr(Class::const_data_member);
      constexpr auto traits = field_traits(__reflect(query_get_decl_traits, refl));

      static_assert(traits.is_mutable == false);
    }
  }

  // access
  {
    // {
    //   constexpr auto refl = reflexpr(Class::default_access);
    //   constexpr auto traits = field_traits(__reflect(query_get_decl_traits, refl));

    //   static_assert(get_access(traits.access) == private_access);
    // }

    // {
    //   constexpr auto refl = reflexpr(Class::public_access);
    //   constexpr auto traits = field_traits(__reflect(query_get_decl_traits, refl));

    //   static_assert(get_access(traits.access) == public_access);
    // }

    // {
    //   constexpr auto refl = reflexpr(Class::protected_access);
    //   constexpr auto traits = field_traits(__reflect(query_get_decl_traits, refl));

    //   static_assert(get_access(traits.access) == protected_access);
    // }

    // {
    //   constexpr auto refl = reflexpr(Class::private_access);
    //   constexpr auto traits = field_traits(__reflect(query_get_decl_traits, refl));

    //   static_assert(get_access(traits.access) == private_access);
    // }
  }

  return 0;
}
