// RUN: %clang_cc1 -freflection -std=c++1z %s

#include "reflection_query.h"
#include "reflection_traits.h"

class ImplicitDefaultCtorClass {
};

class ExplicitDefaultCtorClass {
public:
  ExplicitDefaultCtorClass() = default;
};

class ExplicitCtorClass {
public:
  explicit ExplicitCtorClass() { };
};

class ConstexprCtorClass {
public:
  constexpr ConstexprCtorClass() { };
};

class Class {
  void default_access() { }
public:
  void method() { }
  void undefined_method();
  void noexcept_method() noexcept { }
  inline void inline_method() { }
  constexpr void constexpr_method() { }
  void out_of_line_method();
  void deleted_method() = delete;
  void public_access() { }
protected:
  void protected_access() { }
private:
  void private_access() { }
};

void Class::out_of_line_method() { }

class VirtualClass {
public:
  virtual ~VirtualClass() { }
  virtual void virtual_method() { }
  virtual void pure_virtual_method() = 0;
};

class OverrideChildClass : public VirtualClass {
public:
  void virtual_method() override { }
};

class FinalOverrideChildClass : public VirtualClass {
public:
  void virtual_method() final { }
};

namespace {

class InternalClass {
public:
  void method() { }
};

}

int main() {
  // default ctor
  // {
  //   constexpr auto refl = reflexpr(ImplicitDefaultCtorClass::ImplicitDefaultCtorClass);
  //   constexpr auto traits = method_traits(__reflect(query_get_decl_traits, refl));

  //   static_assert(get_method(traits.kind) == method_ctor);
  //   static_assert(traits.is_constexpr == false);
  //   static_assert(traits.is_explicit == false);
  //   static_assert(traits.is_virtual == false);
  //   static_assert(traits.is_pure == false);
  //   static_assert(traits.is_final == false);
  //   static_assert(traits.is_override == false);
  //   static_assert(traits.is_noexcept == true);
  //   static_assert(traits.is_defined == true);
  //   static_assert(traits.is_inline == false);
  //   static_assert(traits.is_deleted == false);
  //   static_assert(traits.is_defaulted == true);
  //   static_assert(traits.is_trivial == true);
  //   static_assert(traits.is_default_ctor == true);
  //   static_assert(traits.is_copy_ctor == false);
  //   static_assert(traits.is_move_ctor == false);
  //   static_assert(traits.is_copy_assign == false);
  //   static_assert(traits.is_move_assign == false);
  // }

  // default destructor
  // {
  //   constexpr auto refl = reflexpr(ImplicitDefaultCtorClass::~ImplicitDefaultCtorClass);
  //   constexpr auto traits = method_traits(__reflect(query_get_decl_traits, refl));

  //   static_assert(get_method(traits.kind) == method_dtor);
  //   static_assert(traits.is_constexpr == false);
  //   static_assert(traits.is_explicit == false);
  //   static_assert(traits.is_virtual == false);
  //   static_assert(traits.is_pure == false);
  //   static_assert(traits.is_final == false);
  //   static_assert(traits.is_override == false);
  //   static_assert(traits.is_noexcept == true);
  //   static_assert(traits.is_defined == true);
  //   static_assert(traits.is_inline == false);
  //   static_assert(traits.is_deleted == false);
  //   static_assert(traits.is_defaulted == true);
  //   static_assert(traits.is_trivial == true);
  //   static_assert(traits.is_default_ctor == true);
  //   static_assert(traits.is_copy_ctor == false);
  //   static_assert(traits.is_move_ctor == false);
  //   static_assert(traits.is_copy_assign == false);
  //   static_assert(traits.is_move_assign == false);
  // }

  // explicit default constructor
  // {
  //   constexpr auto refl = reflexpr(ExplicitDefaultCtorClass::ExplicitDefaultCtorClass);
  //   constexpr auto traits = method_traits(__reflect(query_get_decl_traits, refl));

  //   static_assert(get_method(traits.kind) == method_ctor);
  //   static_assert(traits.is_noexcept == true);
  //   static_assert(traits.is_defaulted == true);
  //   static_assert(traits.is_default_ctor == true);
  // }

  // explicit explicit constructor
  // {
  //   constexpr auto refl = reflexpr(ExplicitCtorClass::ExplicitCtorClass);
  //   constexpr auto traits = method_traits(__reflect(query_get_decl_traits, refl));

  //   static_assert(get_method(traits.kind) == method_ctor);
  //   static_assert(traits.is_explicit == true);
  //   static_assert(traits.is_default_ctor == false);
  // }

  // constexpr constructor
  // {
  //   constexpr auto refl = reflexpr(ConstexprCtorClass::ConstexprCtorClass);
  //   constexpr auto traits = method_traits(__reflect(query_get_decl_traits, refl));

  //   static_assert(get_method(traits.kind) == method_ctor);
  //   static_assert(traits.is_constexpr == true);
  //   static_assert(traits.is_default_ctor == false);
  // }

  // method of exposed class traits
  {
    constexpr auto refl = reflexpr(Class::method);
    constexpr auto traits = method_traits(__reflect(query_get_decl_traits, refl));

    static_assert(get_linkage(traits.linkage) == external_linkage);
    // static_assert(get_storage(traits.storage) == no_storage);
    static_assert(get_method(traits.kind) == method_normal);
    static_assert(traits.is_constexpr == false);
    static_assert(traits.is_explicit == false);
    static_assert(traits.is_virtual == false);
    static_assert(traits.is_pure == false);
    static_assert(traits.is_final == false);
    static_assert(traits.is_override == false);
    static_assert(traits.is_noexcept == false);
    static_assert(traits.is_defined == true);
    static_assert(traits.is_inline == true);
    static_assert(traits.is_deleted == false);
    static_assert(traits.is_defaulted == false);
    static_assert(traits.is_trivial == false);
    static_assert(traits.is_default_ctor == false);
    static_assert(traits.is_copy_ctor == false);
    static_assert(traits.is_move_ctor == false);
    static_assert(traits.is_copy_assign == false);
    static_assert(traits.is_move_assign == false);
  }

  // method of internal class traits
  {
    constexpr auto refl = reflexpr(InternalClass::method);
    constexpr auto traits = method_traits(__reflect(query_get_decl_traits, refl));

    static_assert(get_linkage(traits.linkage) == internal_linkage);
    // static_assert(get_storage(traits.storage) == no_storage);
    static_assert(get_method(traits.kind) == method_normal);
    static_assert(traits.is_constexpr == false);
    static_assert(traits.is_explicit == false);
    static_assert(traits.is_virtual == false);
    static_assert(traits.is_pure == false);
    static_assert(traits.is_final == false);
    static_assert(traits.is_override == false);
    static_assert(traits.is_noexcept == false);
    static_assert(traits.is_defined == true);
    static_assert(traits.is_inline == true);
    static_assert(traits.is_deleted == false);
    static_assert(traits.is_defaulted == false);
    static_assert(traits.is_trivial == false);
    static_assert(traits.is_default_ctor == false);
    static_assert(traits.is_copy_ctor == false);
    static_assert(traits.is_move_ctor == false);
    static_assert(traits.is_copy_assign == false);
    static_assert(traits.is_move_assign == false);
  }

  // undefined method
  {
    constexpr auto refl = reflexpr(Class::undefined_method);
    constexpr auto traits = method_traits(__reflect(query_get_decl_traits, refl));

    static_assert(traits.is_defined == false);
  }

  // noexpect method
  {
    constexpr auto refl = reflexpr(Class::noexcept_method);
    constexpr auto traits = method_traits(__reflect(query_get_decl_traits, refl));

    static_assert(traits.is_noexcept == true);
  }

  // noexpect method
  {
    constexpr auto refl = reflexpr(Class::noexcept_method);
    constexpr auto traits = method_traits(__reflect(query_get_decl_traits, refl));

    static_assert(traits.is_noexcept == true);
  }

  // inline method
  {
    constexpr auto refl = reflexpr(Class::inline_method);
    constexpr auto traits = method_traits(__reflect(query_get_decl_traits, refl));

    static_assert(traits.is_inline == true);
  }

  // constexpr method
  {
    constexpr auto refl = reflexpr(Class::constexpr_method);
    constexpr auto traits = method_traits(__reflect(query_get_decl_traits, refl));

    static_assert(traits.is_constexpr == true);
  }

  // out of line method
  {
    constexpr auto refl = reflexpr(Class::out_of_line_method);
    constexpr auto traits = method_traits(__reflect(query_get_decl_traits, refl));

    static_assert(traits.is_inline == false);
    static_assert(traits.is_defined == true);
  }

  // deleted method
  // {
  //   constexpr auto refl = reflexpr(Class::deleted_method);
  //   constexpr auto traits = method_traits(__reflect(query_get_decl_traits, refl));

  //   static_assert(traits.is_deleted == true);
  // }

  // virtual destructor
  // {
  //   constexpr auto refl = reflexpr(VirtualClass::~VirtualClass);
  //   constexpr auto traits = method_traits(__reflect(query_get_decl_traits, refl));

  //   static_assert(traits.is_virtual == true);
  // }

  // virtual method
  {
    constexpr auto refl = reflexpr(VirtualClass::virtual_method);
    constexpr auto traits = method_traits(__reflect(query_get_decl_traits, refl));

    static_assert(traits.is_virtual == true);
  }

  // pure virtual method
  {
    constexpr auto refl = reflexpr(VirtualClass::pure_virtual_method);
    constexpr auto traits = method_traits(__reflect(query_get_decl_traits, refl));

    static_assert(traits.is_virtual == true);
    static_assert(traits.is_pure == true);
    static_assert(traits.is_defined == false);
  }

  // override virtual method
  {
    constexpr auto refl = reflexpr(OverrideChildClass::virtual_method);
    constexpr auto traits = method_traits(__reflect(query_get_decl_traits, refl));

    static_assert(traits.is_virtual == true);
    static_assert(traits.is_override == true);
  }

  // final virtual method
  {
    constexpr auto refl = reflexpr(FinalOverrideChildClass::virtual_method);
    constexpr auto traits = method_traits(__reflect(query_get_decl_traits, refl));

    static_assert(traits.is_virtual == true);
    static_assert(traits.is_final == true);
  }

  // access
  {
    // {
    //   constexpr auto refl = reflexpr(Class::default_access);
    //   constexpr auto traits = method_traits(__reflect(query_get_decl_traits, refl));

    //   static_assert(get_access(traits.access) == private_access);
    // }

    // {
    //   constexpr auto refl = reflexpr(Class::public_access);
    //   constexpr auto traits = method_traits(__reflect(query_get_decl_traits, refl));

    //   static_assert(get_access(traits.access) == public_access);
    // }

    // {
    //   constexpr auto refl = reflexpr(Class::protected_access);
    //   constexpr auto traits = method_traits(__reflect(query_get_decl_traits, refl));

    //   static_assert(get_access(traits.access) == protected_access);
    // }

    // {
    //   constexpr auto refl = reflexpr(Class::private_access);
    //   constexpr auto traits = method_traits(__reflect(query_get_decl_traits, refl));

    //   static_assert(get_access(traits.access) == private_access);
    // }
  }

  return 0;
}
