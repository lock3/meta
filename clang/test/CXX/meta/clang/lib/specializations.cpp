// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

namespace partial_specialization {
  namespace refl {
    template<typename A, typename B, typename C>
    class Foo { };

    template<typename B, typename C>
    class Foo<int, B, C> { };
  }

  constexpr auto base_template_refl = __reflect(query_get_begin, reflexpr(refl));

  constexpr auto partial_spec_refl = __reflect(query_get_next, base_template_refl);
  static_assert(__reflect(query_is_template, partial_spec_refl));
  static_assert(__reflect(query_is_specialization, partial_spec_refl));
  static_assert(__reflect(query_is_partial_specialization, partial_spec_refl));
  static_assert(__reflect(query_is_explicit_specialization, partial_spec_refl));
  static_assert(!__reflect(query_is_implicit_instantiation, partial_spec_refl));
  static_assert(!__reflect(query_is_explicit_instantiation, partial_spec_refl));
}

namespace explicit_specialization {
  namespace refl {
    template<typename A, typename B, typename C>
    class Foo { };

    template<>
    class Foo<int, int, int> { };
  }

  constexpr auto base_template_refl = __reflect(query_get_begin, reflexpr(refl));

  constexpr auto explicit_spec_refl = __reflect(query_get_next, base_template_refl);
  static_assert(!__reflect(query_is_template, explicit_spec_refl));
  static_assert(__reflect(query_is_specialization, explicit_spec_refl));
  static_assert(!__reflect(query_is_partial_specialization, explicit_spec_refl));
  static_assert(__reflect(query_is_explicit_specialization, explicit_spec_refl));
  static_assert(!__reflect(query_is_implicit_instantiation, explicit_spec_refl));
  static_assert(!__reflect(query_is_explicit_instantiation, explicit_spec_refl));
}

namespace implicit_instantiation {
  namespace refl {
    template<typename A, typename B, typename C>
    class Foo { };

    Foo<int, int, int> x;
  }

  constexpr auto base_template_refl = __reflect(query_get_begin, reflexpr(refl));

  constexpr auto var_of_implicit_type = __reflect(query_get_next, base_template_refl);
  constexpr auto type_of_var = __reflect(query_get_type, var_of_implicit_type);
  constexpr auto implicit_inst_refl = __reflect(query_get_definition, type_of_var);
  static_assert(!__reflect(query_is_template, implicit_inst_refl));
  static_assert(__reflect(query_is_specialization, implicit_inst_refl));
  static_assert(!__reflect(query_is_partial_specialization, implicit_inst_refl));
  static_assert(!__reflect(query_is_explicit_specialization, implicit_inst_refl));
  static_assert(__reflect(query_is_implicit_instantiation, implicit_inst_refl));
  static_assert(!__reflect(query_is_explicit_instantiation, implicit_inst_refl));
}

namespace explicit_instantiation {
  namespace refl {
    template<typename A, typename B, typename C>
    class Foo { };

    extern template class Foo<int, int, int>;
  }

  constexpr auto base_template_refl = __reflect(query_get_begin, reflexpr(refl));

  constexpr auto explicit_inst_refl = __reflect(query_get_next, base_template_refl);
  static_assert(!__reflect(query_is_template, explicit_inst_refl));
  static_assert(__reflect(query_is_specialization, explicit_inst_refl));
  static_assert(!__reflect(query_is_partial_specialization, explicit_inst_refl));
  static_assert(!__reflect(query_is_explicit_specialization, explicit_inst_refl));
  static_assert(!__reflect(query_is_implicit_instantiation, explicit_inst_refl));
  static_assert(__reflect(query_is_explicit_instantiation, explicit_inst_refl));
}
