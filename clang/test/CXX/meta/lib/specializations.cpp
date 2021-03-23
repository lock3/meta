// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../support/query.h"

namespace partial_specialization {
  namespace class_ns {
    namespace refl {
      template<typename A, typename B, typename C>
      class Foo { };

      template<typename B, typename C>
      class Foo<int, B, C> { };
    }

    constexpr auto base_template_refl = __reflect(query_get_begin, ^refl);
    constexpr auto partial_spec_refl = __reflect(query_get_next, base_template_refl);
    static_assert(__reflect(query_is_template, partial_spec_refl));
    static_assert(__reflect(query_is_specialization, partial_spec_refl));
    static_assert(__reflect(query_is_partial_specialization, partial_spec_refl));
    static_assert(__reflect(query_is_explicit_specialization, partial_spec_refl));
    static_assert(!__reflect(query_is_implicit_instantiation, partial_spec_refl));
    static_assert(!__reflect(query_is_explicit_instantiation, partial_spec_refl));
    static_assert(__reflect(query_is_specialization_of, base_template_refl, partial_spec_refl));

    namespace alt_refl {
      template<typename A, typename B, typename C>
      class Foo { };

      template<typename B, typename C>
      class Foo<int, B, C> { };
    }

    constexpr auto alt_base_template_refl = __reflect(query_get_begin, ^alt_refl);
    constexpr auto alt_partial_spec_refl = __reflect(query_get_next, alt_base_template_refl);
    static_assert(!__reflect(query_is_specialization_of, base_template_refl, alt_partial_spec_refl));
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
    static_assert(__reflect(query_is_template, partial_spec_refl));
    static_assert(__reflect(query_is_specialization, partial_spec_refl));
    static_assert(__reflect(query_is_partial_specialization, partial_spec_refl));
    static_assert(__reflect(query_is_explicit_specialization, partial_spec_refl));
    static_assert(!__reflect(query_is_implicit_instantiation, partial_spec_refl));
    static_assert(!__reflect(query_is_explicit_instantiation, partial_spec_refl));
    static_assert(__reflect(query_is_specialization_of, base_template_refl, partial_spec_refl));

    namespace alt_refl {
      template<typename A, int B>
      A var = B;

      template<int B>
      int var<int, B> = 1;
    }

    constexpr auto alt_base_template_refl = __reflect(query_get_begin, ^alt_refl);
    constexpr auto alt_partial_spec_refl = __reflect(query_get_next, alt_base_template_refl);
    static_assert(!__reflect(query_is_specialization_of, base_template_refl, alt_partial_spec_refl));
  }
}

namespace explicit_specialization {
  namespace class_ns {
    namespace refl {
      template<typename A, typename B, typename C>
      class Foo { };

      template<>
      class Foo<int, int, int> { };
    }

    constexpr auto base_template_refl = __reflect(query_get_begin, ^refl);
    constexpr auto explicit_spec_refl = __reflect(query_get_next, base_template_refl);
    static_assert(!__reflect(query_is_template, explicit_spec_refl));
    static_assert(__reflect(query_is_specialization, explicit_spec_refl));
    static_assert(!__reflect(query_is_partial_specialization, explicit_spec_refl));
    static_assert(__reflect(query_is_explicit_specialization, explicit_spec_refl));
    static_assert(!__reflect(query_is_implicit_instantiation, explicit_spec_refl));
    static_assert(!__reflect(query_is_explicit_instantiation, explicit_spec_refl));
    static_assert(__reflect(query_is_specialization_of, base_template_refl, explicit_spec_refl));

    namespace alt_refl {
      template<typename A, typename B, typename C>
      class Foo { };

      template<>
      class Foo<int, int, int> { };
    }

    constexpr auto alt_base_template_refl = __reflect(query_get_begin, ^alt_refl);
    constexpr auto alt_explicit_spec_refl = __reflect(query_get_next, alt_base_template_refl);
    static_assert(!__reflect(query_is_specialization_of, base_template_refl, alt_explicit_spec_refl));
  }

  namespace fn_ns {
    namespace refl {
      template<typename A, int B>
      int fn() { return 0; }

      template<>
      int fn<int, 2>() { return 1; }
    }

    constexpr auto base_template_refl = __reflect(query_get_begin, ^refl);
    constexpr auto explicit_spec_refl = __reflect(query_get_begin_overload_candidate, ^refl::fn<int, 2>);
    static_assert(!__reflect(query_is_template, explicit_spec_refl));
    static_assert(__reflect(query_is_specialization, explicit_spec_refl));
    static_assert(!__reflect(query_is_partial_specialization, explicit_spec_refl));
    static_assert(__reflect(query_is_explicit_specialization, explicit_spec_refl));
    static_assert(!__reflect(query_is_implicit_instantiation, explicit_spec_refl));
    static_assert(!__reflect(query_is_explicit_instantiation, explicit_spec_refl));
    static_assert(__reflect(query_is_specialization_of, base_template_refl, explicit_spec_refl));

    namespace alt_refl {
      template<typename A, int B>
      int fn() { return 0; }

      template<>
      int fn<int, 2>() { return 1; }
    }

    constexpr auto alt_explicit_spec_refl = __reflect(query_get_begin_overload_candidate, ^alt_refl::fn<int, 2>);
    static_assert(!__reflect(query_is_specialization_of, base_template_refl, alt_explicit_spec_refl));
  }

  namespace member_fn_ns {
    struct refl {
      template<typename A, int B>
      int fn() { return 0; }

      template<>
      int fn<int, 2>() { return 1; }
    };

    constexpr auto base_template_refl = __reflect(query_get_begin, ^refl);
    constexpr auto explicit_spec_refl = __reflect(query_get_begin_overload_candidate, ^refl::fn<int, 2>);
    static_assert(!__reflect(query_is_template, explicit_spec_refl));
    static_assert(__reflect(query_is_specialization, explicit_spec_refl));
    static_assert(!__reflect(query_is_partial_specialization, explicit_spec_refl));
    static_assert(__reflect(query_is_explicit_specialization, explicit_spec_refl));
    static_assert(!__reflect(query_is_implicit_instantiation, explicit_spec_refl));
    static_assert(!__reflect(query_is_explicit_instantiation, explicit_spec_refl));
    static_assert(__reflect(query_is_specialization_of, base_template_refl, explicit_spec_refl));

    struct alt_refl {
      template<typename A, int B>
      int fn() { return 0; }

      template<>
      int fn<int, 2>() { return 1; }
    };

    constexpr auto alt_explicit_spec_refl = __reflect(query_get_begin_overload_candidate, ^alt_refl::fn<int, 2>);
    static_assert(!__reflect(query_is_specialization_of, base_template_refl, alt_explicit_spec_refl));
  }

  namespace static_data_member_ns {
    struct refl {
      template<typename A, int B>
      static const A var = B;

      template<>
      static const int var<int, 1> = 0;
    };

    constexpr auto base_template_refl = __reflect(query_get_begin, ^refl);
    constexpr auto explicit_spec_refl = __reflect(query_get_next, base_template_refl);
    static_assert(!__reflect(query_is_template, explicit_spec_refl));
    static_assert(__reflect(query_is_specialization, explicit_spec_refl));
    static_assert(!__reflect(query_is_partial_specialization, explicit_spec_refl));
    static_assert(__reflect(query_is_explicit_specialization, explicit_spec_refl));
    static_assert(!__reflect(query_is_implicit_instantiation, explicit_spec_refl));
    static_assert(!__reflect(query_is_explicit_instantiation, explicit_spec_refl));
    static_assert(__reflect(query_is_specialization_of, base_template_refl, explicit_spec_refl));

    struct alt_refl {
      template<typename A, int B>
      static const A var = B;

      template<>
      static const int var<int, 1> = 0;
    };

    constexpr auto alt_base_template_refl = __reflect(query_get_begin, ^alt_refl);
    constexpr auto alt_explicit_spec_refl = __reflect(query_get_next, alt_base_template_refl);
    static_assert(!__reflect(query_is_specialization_of, base_template_refl, alt_explicit_spec_refl));
  }

  namespace var_ns {
    namespace refl {
      template<typename A, int B>
      A var = B;

      template<>
      int var<int, 1> = 0;
    }

    constexpr auto base_template_refl = __reflect(query_get_begin, ^refl);
    constexpr auto explicit_spec_refl = __reflect(query_get_next, base_template_refl);
    static_assert(!__reflect(query_is_template, explicit_spec_refl));
    static_assert(__reflect(query_is_specialization, explicit_spec_refl));
    static_assert(!__reflect(query_is_partial_specialization, explicit_spec_refl));
    static_assert(__reflect(query_is_explicit_specialization, explicit_spec_refl));
    static_assert(!__reflect(query_is_implicit_instantiation, explicit_spec_refl));
    static_assert(!__reflect(query_is_explicit_instantiation, explicit_spec_refl));
    static_assert(__reflect(query_is_specialization_of, base_template_refl, explicit_spec_refl));

    namespace alt_refl {
      template<typename A, int B>
      A var = B;

      template<>
      int var<int, 1> = 0;
    }

    constexpr auto alt_base_template_refl = __reflect(query_get_begin, ^alt_refl);
    constexpr auto alt_explicit_spec_refl = __reflect(query_get_next, alt_base_template_refl);
    static_assert(!__reflect(query_is_specialization_of, base_template_refl, alt_explicit_spec_refl));
  }
}

namespace implicit_instantiation {
  namespace class_ns {
    namespace refl {
      template<typename A, typename B, typename C>
      class Foo { };
    }

    constexpr auto base_template_refl = __reflect(query_get_begin, ^refl);
    constexpr auto implicit_inst_refl = ^refl::Foo<int, int, int>;
    static_assert(!__reflect(query_is_template, implicit_inst_refl));
    static_assert(__reflect(query_is_specialization, implicit_inst_refl));
    static_assert(!__reflect(query_is_partial_specialization, implicit_inst_refl));
    static_assert(!__reflect(query_is_explicit_specialization, implicit_inst_refl));
    static_assert(__reflect(query_is_implicit_instantiation, implicit_inst_refl));
    static_assert(!__reflect(query_is_explicit_instantiation, implicit_inst_refl));
    static_assert(__reflect(query_is_specialization_of, base_template_refl, implicit_inst_refl));

    namespace alt_refl {
      template<typename A, typename B, typename C>
      class Foo { };
    }

    constexpr auto alt_implicit_inst_refl = ^alt_refl::Foo<int, int, int>;
    static_assert(!__reflect(query_is_specialization_of, base_template_refl, alt_implicit_inst_refl));
  }

  namespace fn_ns {
    namespace refl {
      template<typename A, int B>
      int fn() { return 0; }
    }

    constexpr auto base_template_refl = __reflect(query_get_begin, ^refl);
    constexpr auto implicit_inst_refl = __reflect(query_get_begin_overload_candidate, ^refl::fn<int, 1>);
    static_assert(!__reflect(query_is_template, implicit_inst_refl));
    static_assert(__reflect(query_is_specialization, implicit_inst_refl));
    static_assert(!__reflect(query_is_partial_specialization, implicit_inst_refl));
    static_assert(!__reflect(query_is_explicit_specialization, implicit_inst_refl));
    static_assert(__reflect(query_is_implicit_instantiation, implicit_inst_refl));
    static_assert(!__reflect(query_is_explicit_instantiation, implicit_inst_refl));
    static_assert(__reflect(query_is_specialization_of, base_template_refl, implicit_inst_refl));

    namespace alt_refl {
      template<typename A, int B>
      int fn() { return 0; }
    }

    constexpr auto alt_implicit_inst_refl = __reflect(query_get_begin_overload_candidate, ^alt_refl::fn<int, 1>);
    static_assert(!__reflect(query_is_specialization_of, base_template_refl, alt_implicit_inst_refl));
  }

  namespace member_fn_ns {
    struct refl {
      template<typename A, int B>
      int fn() { return 0; }
    };

    constexpr auto base_template_refl = __reflect(query_get_begin, ^refl);
    constexpr auto implicit_inst_refl = __reflect(query_get_begin_overload_candidate, ^refl::fn<int, 1>);
    static_assert(!__reflect(query_is_template, implicit_inst_refl));
    static_assert(__reflect(query_is_specialization, implicit_inst_refl));
    static_assert(!__reflect(query_is_partial_specialization, implicit_inst_refl));
    static_assert(!__reflect(query_is_explicit_specialization, implicit_inst_refl));
    static_assert(__reflect(query_is_implicit_instantiation, implicit_inst_refl));
    static_assert(!__reflect(query_is_explicit_instantiation, implicit_inst_refl));
    static_assert(__reflect(query_is_specialization_of, base_template_refl, implicit_inst_refl));

    struct alt_refl {
      template<typename A, int B>
      int fn() { return 0; }
    };

    constexpr auto alt_implicit_inst_refl = __reflect(query_get_begin_overload_candidate, ^alt_refl::fn<int, 1>);
    static_assert(!__reflect(query_is_specialization_of, base_template_refl, alt_implicit_inst_refl));
  }

  namespace static_data_member_ns {
    struct refl {
      template<typename A, int B>
      static const A var = B;
    };

    constexpr auto base_template_refl = __reflect(query_get_begin, ^refl);
    constexpr auto implicit_inst_refl = ^refl::var<int, 1>;
    static_assert(!__reflect(query_is_template, implicit_inst_refl));
    static_assert(__reflect(query_is_specialization, implicit_inst_refl));
    static_assert(!__reflect(query_is_partial_specialization, implicit_inst_refl));
    static_assert(!__reflect(query_is_explicit_specialization, implicit_inst_refl));
    static_assert(__reflect(query_is_implicit_instantiation, implicit_inst_refl));
    static_assert(!__reflect(query_is_explicit_instantiation, implicit_inst_refl));
    static_assert(__reflect(query_is_specialization_of, base_template_refl, implicit_inst_refl));

    struct alt_refl {
      template<typename A, int B>
      static const A var = B;
    };

    constexpr auto alt_implicit_inst_refl = ^alt_refl::var<int, 1>;
    static_assert(!__reflect(query_is_specialization_of, base_template_refl, alt_implicit_inst_refl));
  }

  namespace var_ns {
    namespace refl {
      template<typename A, int B>
      A var = B;
    }

    constexpr auto base_template_refl = __reflect(query_get_begin, ^refl);
    constexpr auto implicit_inst_refl = ^refl::var<int, 1>;
    static_assert(!__reflect(query_is_template, implicit_inst_refl));
    static_assert(__reflect(query_is_specialization, implicit_inst_refl));
    static_assert(!__reflect(query_is_partial_specialization, implicit_inst_refl));
    static_assert(!__reflect(query_is_explicit_specialization, implicit_inst_refl));
    static_assert(__reflect(query_is_implicit_instantiation, implicit_inst_refl));
    static_assert(!__reflect(query_is_explicit_instantiation, implicit_inst_refl));
    static_assert(__reflect(query_is_specialization_of, base_template_refl, implicit_inst_refl));

    namespace alt_refl {
      template<typename A, int B>
      A var = B;
    }

    constexpr auto alt_implicit_inst_refl = ^alt_refl::var<int, 1>;
    static_assert(!__reflect(query_is_specialization_of, base_template_refl, alt_implicit_inst_refl));
  }
}

namespace explicit_instantiation {
  namespace class_ns {
    namespace refl {
      template<typename A, typename B, typename C>
      class Foo { };

      extern template class Foo<int, int, int>;
    }

    constexpr auto base_template_refl = __reflect(query_get_begin, ^refl);
    constexpr auto explicit_inst_refl = __reflect(query_get_next, base_template_refl);
    static_assert(!__reflect(query_is_template, explicit_inst_refl));
    static_assert(__reflect(query_is_specialization, explicit_inst_refl));
    static_assert(!__reflect(query_is_partial_specialization, explicit_inst_refl));
    static_assert(!__reflect(query_is_explicit_specialization, explicit_inst_refl));
    static_assert(!__reflect(query_is_implicit_instantiation, explicit_inst_refl));
    static_assert(__reflect(query_is_explicit_instantiation, explicit_inst_refl));
    static_assert(__reflect(query_is_specialization_of, base_template_refl, explicit_inst_refl));

    namespace alt_refl {
      template<typename A, typename B, typename C>
      class Foo { };

      extern template class Foo<int, int, int>;
    }

    constexpr auto alt_base_template_refl = __reflect(query_get_begin, ^alt_refl);
    constexpr auto alt_explicit_inst_refl = __reflect(query_get_next, alt_base_template_refl);
    static_assert(!__reflect(query_is_specialization_of, base_template_refl, alt_explicit_inst_refl));
  }

  namespace fn_ns {
    namespace refl {
      template<typename A, int B>
      int fn() { return 0; }

      extern template int fn<int, 2>();
    }

    constexpr auto base_template_refl = __reflect(query_get_begin, ^refl);
    constexpr auto explicit_inst_refl = __reflect(query_get_begin_overload_candidate, ^refl::fn<int, 2>);
    static_assert(!__reflect(query_is_template, explicit_inst_refl));
    static_assert(__reflect(query_is_specialization, explicit_inst_refl));
    static_assert(!__reflect(query_is_partial_specialization, explicit_inst_refl));
    static_assert(!__reflect(query_is_explicit_specialization, explicit_inst_refl));
    static_assert(!__reflect(query_is_implicit_instantiation, explicit_inst_refl));
    static_assert(__reflect(query_is_explicit_instantiation, explicit_inst_refl));
    static_assert(__reflect(query_is_specialization_of, base_template_refl, explicit_inst_refl));


    namespace alt_refl {
      template<typename A, int B>
      int fn() { return 0; }
    }

    constexpr auto alt_explicit_inst_refl = __reflect(query_get_begin_overload_candidate, ^alt_refl::fn<int, 2>);
    static_assert(!__reflect(query_is_specialization_of, base_template_refl, alt_explicit_inst_refl));
  }

  namespace member_fn_ns {
    namespace refl {
      struct refl_class {
        template<typename A, int B>
        int fn() { return 0; }
      };

      extern template int refl_class::fn<int, 2>();
    }

    constexpr auto base_template_refl = __reflect(query_get_begin, ^refl::refl_class);
    constexpr auto explicit_inst_refl = __reflect(query_get_begin_overload_candidate, ^refl::refl_class::fn<int, 2>);
    static_assert(!__reflect(query_is_template, explicit_inst_refl));
    static_assert(__reflect(query_is_specialization, explicit_inst_refl));
    static_assert(!__reflect(query_is_partial_specialization, explicit_inst_refl));
    static_assert(!__reflect(query_is_explicit_specialization, explicit_inst_refl));
    static_assert(!__reflect(query_is_implicit_instantiation, explicit_inst_refl));
    static_assert(__reflect(query_is_explicit_instantiation, explicit_inst_refl));
    static_assert(__reflect(query_is_specialization_of, base_template_refl, explicit_inst_refl));

    namespace alt_refl {
      struct refl_class {
        template<typename A, int B>
        int fn() { return 0; }
      };
    }

    constexpr auto alt_explicit_inst_refl = __reflect(query_get_begin_overload_candidate, ^alt_refl::refl_class::fn<int, 2>);
    static_assert(!__reflect(query_is_specialization_of, base_template_refl, alt_explicit_inst_refl));
  }

  namespace static_data_member_ns {
    namespace refl {
      struct refl_class {
        template<typename A, int B>
        static const A var = B;
      };

      extern template const int refl_class::var<int, 2>;
    }

    constexpr auto base_template_refl = __reflect(query_get_begin, ^refl::refl_class);
    constexpr auto explicit_inst_refl = ^refl::refl_class::var<int, 2>;
    static_assert(!__reflect(query_is_template, explicit_inst_refl));
    static_assert(__reflect(query_is_specialization, explicit_inst_refl));
    static_assert(!__reflect(query_is_partial_specialization, explicit_inst_refl));
    static_assert(!__reflect(query_is_explicit_specialization, explicit_inst_refl));
    static_assert(!__reflect(query_is_implicit_instantiation, explicit_inst_refl));
    static_assert(__reflect(query_is_explicit_instantiation, explicit_inst_refl));
    static_assert(__reflect(query_is_specialization_of, base_template_refl, explicit_inst_refl));

    namespace alt_refl {
      struct refl_class {
        template<typename A, int B>
        static const A var = B;
      };
    }

    constexpr auto alt_explicit_inst_refl = ^alt_refl::refl_class::var<int, 2>;
    static_assert(!__reflect(query_is_specialization_of, base_template_refl, alt_explicit_inst_refl));
  }

  namespace var_ns {
    namespace refl {
      template<typename A, int B>
      A var = B;

      extern template int var<int, 2>;
    }

    constexpr auto base_template_refl = __reflect(query_get_begin, ^refl);
    constexpr auto explicit_inst_refl = __reflect(query_get_next, base_template_refl);
    static_assert(!__reflect(query_is_template, explicit_inst_refl));
    static_assert(__reflect(query_is_specialization, explicit_inst_refl));
    static_assert(!__reflect(query_is_partial_specialization, explicit_inst_refl));
    static_assert(!__reflect(query_is_explicit_specialization, explicit_inst_refl));
    static_assert(!__reflect(query_is_implicit_instantiation, explicit_inst_refl));
    static_assert(__reflect(query_is_explicit_instantiation, explicit_inst_refl));
    static_assert(__reflect(query_is_specialization_of, base_template_refl, explicit_inst_refl));

    namespace alt_refl {
      template<typename A, int B>
      A var = B;

      extern template int var<int, 2>;
    }

    constexpr auto alt_base_template_refl = __reflect(query_get_begin, ^alt_refl);
    constexpr auto alt_explicit_inst_refl = __reflect(query_get_next, alt_base_template_refl);
    static_assert(!__reflect(query_is_specialization_of, base_template_refl, alt_explicit_inst_refl));
  }
}
