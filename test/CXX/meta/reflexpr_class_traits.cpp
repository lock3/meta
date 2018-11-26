// RUN: %clang_cc1 -freflection -std=c++1z %s

#include "reflection_query.h"
#include "reflection_traits.h"

class SimpleClass {
};

class IncompleteClass;

class NestedClassHolder {
public:
  class NestedClass {
  };
};

class PopulatedClass {
  int population = 42;
};

class AbstractClass {
  virtual void do_things() = 0;
};

class PolymorphicClass {
public:
  virtual ~PolymorphicClass() { };
};

class FinalChildClass final : public PolymorphicClass {
public:
  virtual ~FinalChildClass() { };
};

class ClassClass { };
struct StructClass { };
union UnionClass { };

namespace {
  class InternalClass {
  };
}

int main() {
  // simple empty class traits
  {
    constexpr auto refl = reflexpr(SimpleClass);
    constexpr auto traits = class_traits(__reflect(query_get_type_traits, refl));

    static_assert(traits.linkage == external_linkage);
    static_assert(traits.access == no_access);
    static_assert(traits.is_complete == true);
    static_assert(traits.is_polymorphic == false);
    static_assert(traits.is_abstract == false);
    static_assert(traits.is_final == false);
    static_assert(traits.is_empty == true);
  }

  // incomplete class traits
  {
    constexpr auto refl = reflexpr(IncompleteClass);
    constexpr auto traits = class_traits(__reflect(query_get_type_traits, refl));

    static_assert(traits.linkage == external_linkage);
    static_assert(traits.access == no_access);
    static_assert(traits.is_complete == false);
    static_assert(traits.is_polymorphic == false);
    static_assert(traits.is_abstract == false);
    static_assert(traits.is_final == false);
    static_assert(traits.is_empty == false);
  }

  // nested class traits
  {
    constexpr auto refl = reflexpr(NestedClassHolder::NestedClass);
    constexpr auto traits = class_traits(__reflect(query_get_type_traits, refl));

    static_assert(traits.linkage == external_linkage);
    static_assert(traits.access == public_access);
    static_assert(traits.is_complete == true);
    static_assert(traits.is_polymorphic == false);
    static_assert(traits.is_abstract == false);
    static_assert(traits.is_final == false);
    static_assert(traits.is_empty == true);
  }

  // populated class traits
  {
    constexpr auto refl = reflexpr(PopulatedClass);
    constexpr auto traits = class_traits(__reflect(query_get_type_traits, refl));

    static_assert(traits.linkage == external_linkage);
    static_assert(traits.access == no_access);
    static_assert(traits.is_complete == true);
    static_assert(traits.is_polymorphic == false);
    static_assert(traits.is_abstract == false);
    static_assert(traits.is_final == false);
    static_assert(traits.is_empty == false);
  }

  // abstract class traits
  {
    constexpr auto refl = reflexpr(AbstractClass);
    constexpr auto traits = class_traits(__reflect(query_get_type_traits, refl));

    static_assert(traits.linkage == external_linkage);
    static_assert(traits.access == no_access);
    static_assert(traits.is_complete == true);
    static_assert(traits.is_polymorphic == true);
    static_assert(traits.is_abstract == true);
    static_assert(traits.is_final == false);
    static_assert(traits.is_empty == false);
  }

  // polymorphic class traits
  {
    constexpr auto refl = reflexpr(PolymorphicClass);
    constexpr auto traits = class_traits(__reflect(query_get_type_traits, refl));

    static_assert(traits.linkage == external_linkage);
    static_assert(traits.access == no_access);
    static_assert(traits.is_complete == true);
    static_assert(traits.is_polymorphic == true);
    static_assert(traits.is_abstract == false);
    static_assert(traits.is_final == false);
    static_assert(traits.is_empty == false);
  }

  // final child class traits
  {
    constexpr auto refl = reflexpr(FinalChildClass);
    constexpr auto traits = class_traits(__reflect(query_get_type_traits, refl));

    static_assert(traits.linkage == external_linkage);
    static_assert(traits.access == no_access);
    static_assert(traits.is_complete == true);
    static_assert(traits.is_polymorphic == true);
    static_assert(traits.is_abstract == false);
    static_assert(traits.is_final == true);
    static_assert(traits.is_empty == false);
  }

  // internal class traits
  {
    constexpr auto refl = reflexpr(InternalClass);
    constexpr auto traits = class_traits(__reflect(query_get_type_traits, refl));

    static_assert(traits.linkage == internal_linkage);
    static_assert(traits.access == no_access);
    static_assert(traits.is_complete == true);
    static_assert(traits.is_polymorphic == false);
    static_assert(traits.is_abstract == false);
    static_assert(traits.is_final == false);
    static_assert(traits.is_empty == true);
  }

  // class kind
  {
    // class class traits
    {
      constexpr auto refl = reflexpr(ClassClassTraits);
      constexpr auto traits = class_traits(__reflect(query_get_type_traits, refl));

      static_assert(traits.kind == class_kind);
    }

    // struct class traits
    {
      constexpr auto refl = reflexpr(StructClassTraits);
      constexpr auto traits = class_traits(__reflect(query_get_type_traits, refl));

      static_assert(traits.kind == struct_kind);
    }

    // union class traits
    {
      constexpr auto refl = reflexpr(UnionClassTraits);
      constexpr auto traits = class_traits(__reflect(query_get_type_traits, refl));

      static_assert(traits.kind == union_kind);
    }
  }

  return 0;
}
