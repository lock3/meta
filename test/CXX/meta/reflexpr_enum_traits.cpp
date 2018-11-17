// RUN: %clang_cc1 -freflection -std=c++1z %s

#include "reflection_query.h"
#include "reflection_traits.h"

enum SimpleEnum { A };

enum class ScopedEnum { A };

enum IncompleteEnum;

class Class {
public:
  enum class NestedEnum { A };
};

namespace {
  enum class InternalEnum { A };
}

int main() {
  // simple enum traits
  {
    constexpr auto refl = reflexpr(SimpleEnum);
    constexpr auto traits = enum_traits(__reflect(query_get_type_traits, refl));

    static_assert(traits.linkage == external_linkage);
    static_assert(traits.access == no_access);
    static_assert(traits.is_scoped == false);
    static_assert(traits.is_complete == true);
  }

  // scoped enum traits
  {
    constexpr auto refl = reflexpr(ScopedEnum);
    constexpr auto traits = enum_traits(__reflect(query_get_type_traits, refl));

    static_assert(traits.linkage == external_linkage);
    static_assert(traits.access == no_access);
    static_assert(traits.is_scoped == true);
    static_assert(traits.is_complete == true);
  }

  // incomplete enum traits
  {
    constexpr auto refl = reflexpr(IncompleteEnum);
    constexpr auto traits = enum_traits(__reflect(query_get_type_traits, refl));

    static_assert(traits.linkage == external_linkage);
    static_assert(traits.access == no_access);
    static_assert(traits.is_scoped == false);
    static_assert(traits.is_complete == false);
  }

  // incomplete enum traits
  {
    constexpr auto refl = reflexpr(IncompleteEnum);
    constexpr auto traits = enum_traits(__reflect(query_get_type_traits, refl));

    static_assert(traits.linkage == external_linkage);
    static_assert(traits.access == no_access);
    static_assert(traits.is_scoped == false);
    static_assert(traits.is_complete == false);
  }

  // nested enum traits
  {
    constexpr auto refl = reflexpr(Class::NestedEnum);
    constexpr auto traits = enum_traits(__reflect(query_get_type_traits, refl));

    static_assert(traits.linkage == external_linkage);
    static_assert(traits.access == public_access);
    static_assert(traits.is_scoped == false);
    static_assert(traits.is_complete == true);
  }

  // internal enum traits
  {
    constexpr auto refl = reflexpr(InternalEnum);
    constexpr auto traits = class_traits(__reflect(query_get_type_traits, refl));

    static_assert(traits.linkage == internal_linkage);
    static_assert(traits.access == no_access);
    static_assert(traits.is_scoped == false);
    static_assert(traits.is_complete == true);
  }

  return 0;
}
