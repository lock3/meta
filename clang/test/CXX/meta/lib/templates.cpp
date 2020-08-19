// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

namespace class_template {
  template<typename T>
  class Foo { };
}

constexpr auto class_templ_refl = __reflect(query_get_begin, reflexpr(class_template));
static_assert(__reflect(query_is_template, class_templ_refl));
static_assert(__reflect(query_is_class_template, class_templ_refl));
static_assert(!__reflect(query_is_alias_template, class_templ_refl));
static_assert(!__reflect(query_is_function_template, class_templ_refl));
static_assert(!__reflect(query_is_variable_template, class_templ_refl));
static_assert(!__reflect(query_is_static_member_function_template, class_templ_refl));
static_assert(!__reflect(query_is_nonstatic_member_function_template, class_templ_refl));
static_assert(!__reflect(query_is_constructor_template, class_templ_refl));
static_assert(!__reflect(query_is_destructor_template, class_templ_refl));
static_assert(!__reflect(query_is_concept, class_templ_refl));

namespace fn_template {
  template<typename T>
  void foo(T x) { }
}

constexpr auto fn_templ_refl = __reflect(query_get_begin, reflexpr(fn_template));
static_assert(__reflect(query_is_template, fn_templ_refl));
static_assert(!__reflect(query_is_class_template, fn_templ_refl));
static_assert(!__reflect(query_is_alias_template, fn_templ_refl));
static_assert(__reflect(query_is_function_template, fn_templ_refl));
static_assert(!__reflect(query_is_variable_template, fn_templ_refl));
static_assert(!__reflect(query_is_static_member_function_template, fn_templ_refl));
static_assert(!__reflect(query_is_nonstatic_member_function_template, fn_templ_refl));
static_assert(!__reflect(query_is_constructor_template, fn_templ_refl));
static_assert(!__reflect(query_is_destructor_template, fn_templ_refl));
static_assert(!__reflect(query_is_concept, fn_templ_refl));

namespace var_template {
  template<typename T>
  T x;
}

constexpr auto var_templ_refl = __reflect(query_get_begin, reflexpr(var_template));
static_assert(__reflect(query_is_template, var_templ_refl));
static_assert(!__reflect(query_is_class_template, var_templ_refl));
static_assert(!__reflect(query_is_alias_template, var_templ_refl));
static_assert(!__reflect(query_is_function_template, var_templ_refl));
static_assert(__reflect(query_is_variable_template, var_templ_refl));
static_assert(!__reflect(query_is_static_member_function_template, var_templ_refl));
static_assert(!__reflect(query_is_nonstatic_member_function_template, var_templ_refl));
static_assert(!__reflect(query_is_constructor_template, var_templ_refl));
static_assert(!__reflect(query_is_destructor_template, var_templ_refl));
static_assert(!__reflect(query_is_concept, var_templ_refl));

namespace normal_class {
  class clazz {
    clazz() { }
    ~clazz() { }

    template<typename T>
    static void bar() { }

    template<typename T>
    void foo() { }

    template<typename T>
    class nested_class { };
  };

  constexpr auto member_1 = __reflect(query_get_begin, reflexpr(clazz));
  static_assert(!__reflect(query_is_template, member_1));
  static_assert(!__reflect(query_is_class_template, member_1));
  static_assert(!__reflect(query_is_alias_template, member_1));
  static_assert(!__reflect(query_is_function_template, member_1));
  static_assert(!__reflect(query_is_variable_template, member_1));
  static_assert(!__reflect(query_is_static_member_function_template, member_1));
  static_assert(!__reflect(query_is_nonstatic_member_function_template, member_1));
  static_assert(!__reflect(query_is_constructor_template, member_1));
  static_assert(!__reflect(query_is_destructor_template, member_1));
  static_assert(!__reflect(query_is_concept, member_1));

  constexpr auto member_2 = __reflect(query_get_next, member_1);
  static_assert(!__reflect(query_is_template, member_2));
  static_assert(!__reflect(query_is_class_template, member_2));
  static_assert(!__reflect(query_is_alias_template, member_2));
  static_assert(!__reflect(query_is_function_template, member_2));
  static_assert(!__reflect(query_is_variable_template, member_2));
  static_assert(!__reflect(query_is_static_member_function_template, member_2));
  static_assert(!__reflect(query_is_nonstatic_member_function_template, member_2));
  static_assert(!__reflect(query_is_constructor_template, member_2));
  static_assert(!__reflect(query_is_destructor_template, member_2));
  static_assert(!__reflect(query_is_concept, member_2));

  constexpr auto member_3 = __reflect(query_get_next, member_2);
  static_assert(__reflect(query_is_template, member_3));
  static_assert(!__reflect(query_is_class_template, member_3));
  static_assert(!__reflect(query_is_alias_template, member_3));
  static_assert(__reflect(query_is_function_template, member_3));
  static_assert(!__reflect(query_is_variable_template, member_3));
  static_assert(__reflect(query_is_static_member_function_template, member_3));
  static_assert(!__reflect(query_is_nonstatic_member_function_template, member_3));
  static_assert(!__reflect(query_is_constructor_template, member_3));
  static_assert(!__reflect(query_is_destructor_template, member_3));
  static_assert(!__reflect(query_is_concept, member_3));

  constexpr auto member_4 = __reflect(query_get_next, member_3);
  static_assert(__reflect(query_is_template, member_4));
  static_assert(!__reflect(query_is_class_template, member_4));
  static_assert(!__reflect(query_is_alias_template, member_4));
  static_assert(__reflect(query_is_function_template, member_4));
  static_assert(!__reflect(query_is_variable_template, member_4));
  static_assert(!__reflect(query_is_static_member_function_template, member_4));
  static_assert(__reflect(query_is_nonstatic_member_function_template, member_4));
  static_assert(!__reflect(query_is_constructor_template, member_4));
  static_assert(!__reflect(query_is_destructor_template, member_4));
  static_assert(!__reflect(query_is_concept, member_4));

  constexpr auto member_5 = __reflect(query_get_next, member_4);
  static_assert(__reflect(query_is_template, member_5));
  static_assert(__reflect(query_is_class_template, member_5));
  static_assert(!__reflect(query_is_alias_template, member_5));
  static_assert(!__reflect(query_is_function_template, member_5));
  static_assert(!__reflect(query_is_variable_template, member_5));
  static_assert(!__reflect(query_is_static_member_function_template, member_5));
  static_assert(!__reflect(query_is_nonstatic_member_function_template, member_5));
  static_assert(!__reflect(query_is_constructor_template, member_5));
  static_assert(!__reflect(query_is_destructor_template, member_5));
  static_assert(!__reflect(query_is_concept, member_5));
}

namespace templ_class {
  namespace container {
    template<typename T>
    class clazz {
      clazz() { }
      ~clazz() { }

      static void bar() { }
      void foo() { }

      class nested_class { };
    };
  }

  constexpr auto member_1 = __reflect(query_get_begin, __reflect(query_get_begin, reflexpr(container)));
  static_assert(__reflect(query_is_template, member_1));
  static_assert(!__reflect(query_is_class_template, member_1));
  static_assert(!__reflect(query_is_alias_template, member_1));
  static_assert(__reflect(query_is_function_template, member_1));
  static_assert(!__reflect(query_is_variable_template, member_1));
  static_assert(!__reflect(query_is_static_member_function_template, member_1));
  static_assert(__reflect(query_is_nonstatic_member_function_template, member_1));
  static_assert(__reflect(query_is_constructor_template, member_1));
  static_assert(!__reflect(query_is_destructor_template, member_1));
  static_assert(!__reflect(query_is_concept, member_1));

  constexpr auto member_2 = __reflect(query_get_next, member_1);
  static_assert(__reflect(query_is_template, member_2));
  static_assert(!__reflect(query_is_class_template, member_2));
  static_assert(!__reflect(query_is_alias_template, member_2));
  static_assert(__reflect(query_is_function_template, member_2));
  static_assert(!__reflect(query_is_variable_template, member_2));
  static_assert(!__reflect(query_is_static_member_function_template, member_2));
  static_assert(__reflect(query_is_nonstatic_member_function_template, member_2));
  static_assert(!__reflect(query_is_constructor_template, member_2));
  static_assert(__reflect(query_is_destructor_template, member_2));
  static_assert(!__reflect(query_is_concept, member_2));

  constexpr auto member_3 = __reflect(query_get_next, member_2);
  static_assert(__reflect(query_is_template, member_3));
  static_assert(!__reflect(query_is_class_template, member_3));
  static_assert(!__reflect(query_is_alias_template, member_3));
  static_assert(__reflect(query_is_function_template, member_3));
  static_assert(!__reflect(query_is_variable_template, member_3));
  static_assert(__reflect(query_is_static_member_function_template, member_3));
  static_assert(!__reflect(query_is_nonstatic_member_function_template, member_3));
  static_assert(!__reflect(query_is_constructor_template, member_3));
  static_assert(!__reflect(query_is_destructor_template, member_3));
  static_assert(!__reflect(query_is_concept, member_3));

  constexpr auto member_4 = __reflect(query_get_next, member_3);
  static_assert(__reflect(query_is_template, member_4));
  static_assert(!__reflect(query_is_class_template, member_4));
  static_assert(!__reflect(query_is_alias_template, member_4));
  static_assert(__reflect(query_is_function_template, member_4));
  static_assert(!__reflect(query_is_variable_template, member_4));
  static_assert(!__reflect(query_is_static_member_function_template, member_4));
  static_assert(__reflect(query_is_nonstatic_member_function_template, member_4));
  static_assert(!__reflect(query_is_constructor_template, member_4));
  static_assert(!__reflect(query_is_destructor_template, member_4));
  static_assert(!__reflect(query_is_concept, member_4));

  constexpr auto member_5 = __reflect(query_get_next, member_4);
  static_assert(__reflect(query_is_template, member_5));
  static_assert(__reflect(query_is_class_template, member_5));
  static_assert(!__reflect(query_is_alias_template, member_5));
  static_assert(!__reflect(query_is_function_template, member_5));
  static_assert(!__reflect(query_is_variable_template, member_5));
  static_assert(!__reflect(query_is_static_member_function_template, member_5));
  static_assert(!__reflect(query_is_nonstatic_member_function_template, member_5));
  static_assert(!__reflect(query_is_constructor_template, member_5));
  static_assert(!__reflect(query_is_destructor_template, member_5));
  static_assert(!__reflect(query_is_concept, member_5));
}
