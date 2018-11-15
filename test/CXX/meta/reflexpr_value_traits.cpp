// RUN: %clang_cc1 -freflection -std=c++1z %s

#include "reflection_query.h"
#include "reflection_traits.h"

enum Enum { A };

namespace {
  enum InnerEnum { InnerA };
}

class Class {
  enum DefaultAccessEnum { DefaultAccessA };
public:
  enum PublicAccessEnum { PublicAccessA };
protected:
  enum ProtectedAccessEnum { ProtectedAccessA };
private:
  enum PrivateAccessEnum { PrivateAccessA };
};


int main() {
  // enum triats
  {
    constexpr auto refl = reflexpr(A);
    constexpr auto traits = value_traits(__reflect(query_get_decl_traits, refl));

    static_assert(get_linkage(traits.linkage) == external_linkage);
    static_assert(get_access(traits.access) == no_access);
  }

  // inner enum traits
  {
    constexpr auto refl = reflexpr(InnerA);
    constexpr auto traits = value_traits(__reflect(query_get_decl_traits, refl));

    static_assert(get_linkage(traits.linkage) == internal_linkage);
    static_assert(get_access(traits.access) == no_access);
  }

  // nested enum access levels
  {
    // {
    //   constexpr auto refl = reflexpr(Class::DefaultAccessA);
    //   constexpr auto traits = variable_traits(__reflect(query_get_decl_traits, refl));

    //   static_assert(get_access(traits.access) == private_access);
    // }

    // {
    //   constexpr auto refl = reflexpr(Class::PublicAccessA);
    //   constexpr auto traits = variable_traits(__reflect(query_get_decl_traits, refl));

    //   static_assert(get_access(traits.access) == public_access);
    // }

    // {
    //   constexpr auto refl = reflexpr(Class::ProtectedAccessA);
    //   constexpr auto traits = variable_traits(__reflect(query_get_decl_traits, refl));

    //   static_assert(get_access(traits.access) == protected_access);
    // }

    // {
    //   constexpr auto refl = reflexpr(Class::PrivateAccessA);
    //   constexpr auto traits = variable_traits(__reflect(query_get_decl_traits, refl));

    //   static_assert(get_access(traits.access) == private_access);
    // }
  }

  return 0;
}
