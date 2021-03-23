// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../support/query.h"

namespace get_template {
  namespace class_ns {
    namespace refl {
      template<typename A, int B>
      class Foo { };

      template<int B>
      class Foo<int, B> { };
    }

    constexpr auto base_template_refl = __reflect(query_get_begin, ^refl);
    constexpr auto partial_spec_refl = __reflect(query_get_next, base_template_refl);

    constexpr auto specialization_refl = ^refl::Foo<int, 1>;
    static_assert(partial_spec_refl == __reflect(query_get_template, specialization_refl));
    static_assert(base_template_refl == __reflect(query_get_template, __reflect(query_get_template, specialization_refl)));

    namespace alt_refl {
      template<typename A, int B>
      class Bar { };
    }

    constexpr auto alt_specialization_refl = ^alt_refl::Bar<int, 1>;
    static_assert(partial_spec_refl != __reflect(query_get_template, alt_specialization_refl));
    static_assert(base_template_refl != __reflect(query_get_template, alt_specialization_refl));
  }

  namespace var_ns {
    namespace refl {
      template<typename A, int B>
      A var = B;

      template<int B>
      int var<int, B> = 1;
    }

    constexpr auto base_template_refl = __reflect(query_get_begin, ^refl);
    constexpr auto partial_spec_refl = __reflect(query_get_next, base_template_refl);

    constexpr auto specialization_refl = ^refl::var<int, 1>;
    static_assert(partial_spec_refl == __reflect(query_get_template, specialization_refl));
    static_assert(base_template_refl == __reflect(query_get_template, __reflect(query_get_template, specialization_refl)));

    namespace alt_refl {
      template<typename A, int B>
      A var = B;
    }

    constexpr auto alt_specialization_refl = ^alt_refl::var<int, 1>;
    static_assert(partial_spec_refl != __reflect(query_get_template, alt_specialization_refl));
    static_assert(base_template_refl != __reflect(query_get_template, alt_specialization_refl));
  }

  namespace fn_ns {
    namespace refl {
      template<typename A, int B>
      int fn() { return 0; }
    }

    constexpr auto base_template_refl = __reflect(query_get_begin, ^refl);
    constexpr auto specialization_refl = __reflect(query_get_begin_overload_candidate, ^refl::fn<int, 1>);
    static_assert(base_template_refl == __reflect(query_get_template, specialization_refl));

    namespace alt_refl {
      template<typename A, int B>
      int fn() { return 0; }
    }

    constexpr auto alt_specialization_refl = __reflect(query_get_begin_overload_candidate, ^alt_refl::fn<int, 1>);
    static_assert(base_template_refl != __reflect(query_get_template, alt_specialization_refl));
  }

  namespace member_fn_ns {
    struct refl {
      template<typename A, int B>
      int fn() { return 0; }
    };

    constexpr auto base_template_refl = __reflect(query_get_begin, ^refl);
    constexpr auto specialization_refl = __reflect(query_get_begin_overload_candidate, ^refl::fn<int, 1>);
    static_assert(base_template_refl == __reflect(query_get_template, specialization_refl));

    struct alt_refl {
      template<typename A, int B>
      int fn() { return 0; }
    };

    constexpr auto alt_specialization_refl = __reflect(query_get_begin_overload_candidate, ^alt_refl::fn<int, 1>);
    static_assert(base_template_refl != __reflect(query_get_template, alt_specialization_refl));
  }

  namespace static_data_member_ns {
    struct refl {
      template<typename A, int B>
      static const A var = B;
    };

    constexpr auto base_template_refl = __reflect(query_get_begin, ^refl);
    constexpr auto specialization_refl = ^refl::var<int, 1>;
    static_assert(base_template_refl == __reflect(query_get_template, specialization_refl));

    struct alt_refl {
      template<typename A, int B>
      static const A var = B;
    };

    constexpr auto alt_specialization_refl = ^alt_refl::var<int, 1>;
    static_assert(base_template_refl != __reflect(query_get_template, alt_specialization_refl));
  }
}
