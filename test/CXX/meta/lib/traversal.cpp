// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

using string_type = const char *;

constexpr bool string_eq(string_type s1, string_type s2) {
  while (*s1 != '\0' && *s1 == *s2) {
    s1++;
    s2++;
  }

  return *s1 == *s2;
}

consteval string_type name_of(meta::info reflection) {
  return __reflect(query_get_name, reflection);
}

consteval bool is_invalid(meta::info reflection) {
  return __reflect(query_is_invalid, reflection);
}

namespace class_mem {
  class clazz {
    int a = 0;
    void b() {
    }
    using c = int;
    using d = float;
  };
}

consteval void member_range_test() {
  constexpr meta::info class_refl = __reflect(query_get_begin_member, reflexpr(class_mem));
  {
    constexpr meta::info first_member_refl = __reflect(query_get_begin_member, class_refl);
    static_assert(string_eq(name_of(first_member_refl), "a"));

    constexpr meta::info second_member_refl = __reflect(query_get_next_member, first_member_refl);
    static_assert(string_eq(name_of(second_member_refl), "b"));

    constexpr meta::info third_member_refl = __reflect(query_get_next_member, second_member_refl);
    static_assert(string_eq(name_of(third_member_refl), "c"));

    constexpr meta::info fourth_member_refl = __reflect(query_get_next_member, third_member_refl);
    static_assert(string_eq(name_of(fourth_member_refl), "d"));

    constexpr meta::info end = __reflect(query_get_next_member, fourth_member_refl);
    static_assert(is_invalid(end));
  }

  // Legacy
  {
    constexpr meta::info first_member_refl = __reflect(query_get_begin, class_refl);
    static_assert(string_eq(name_of(first_member_refl), "a"));

    constexpr meta::info second_member_refl = __reflect(query_get_next, first_member_refl);
    static_assert(string_eq(name_of(second_member_refl), "b"));

    constexpr meta::info third_member_refl = __reflect(query_get_next, second_member_refl);
    static_assert(string_eq(name_of(third_member_refl), "c"));

    constexpr meta::info fourth_member_refl = __reflect(query_get_next, third_member_refl);
    static_assert(string_eq(name_of(fourth_member_refl), "d"));

    constexpr meta::info end = __reflect(query_get_next, fourth_member_refl);
    static_assert(is_invalid(end));
  }
}

namespace templ_class_mem {
  template<typename templ_param_a, typename templ_param_b>
  class clazz {
    int a = 0;
    void b() {
    }
  };
}

consteval void templ_member_range_test() {
  constexpr meta::info templ_class_refl = __reflect(query_get_begin_member, reflexpr(templ_class_mem));
  {
    constexpr meta::info first_member_refl = __reflect(query_get_begin_member, templ_class_refl);
    static_assert(string_eq(name_of(first_member_refl), "a"));

    constexpr meta::info second_member_refl = __reflect(query_get_next_member, first_member_refl);
    static_assert(string_eq(name_of(second_member_refl), "b"));

    constexpr meta::info end = __reflect(query_get_next_member, second_member_refl);
    static_assert(is_invalid(end));
  }
  {
    constexpr meta::info first_templ_param_refl = __reflect(query_get_begin_template_param, templ_class_refl);
    static_assert(string_eq(name_of(first_templ_param_refl), "templ_param_a"));

    constexpr meta::info second_templ_param_refl = __reflect(query_get_next_template_param, first_templ_param_refl);
    static_assert(string_eq(name_of(second_templ_param_refl), "templ_param_b"));

    constexpr meta::info end = __reflect(query_get_next_template_param, second_templ_param_refl);
    static_assert(is_invalid(end));
  }

  // Legacy
  {
    constexpr meta::info first_member_refl = __reflect(query_get_begin, templ_class_refl);
    static_assert(string_eq(name_of(first_member_refl), "a"));

    constexpr meta::info second_member_refl = __reflect(query_get_next, first_member_refl);
    static_assert(string_eq(name_of(second_member_refl), "b"));

    constexpr meta::info end = __reflect(query_get_next, second_member_refl);
    static_assert(is_invalid(end));
  }
}

namespace fn {
  void foo(int a, int b) {
  }
}

constexpr void templ_fn_test() {
  constexpr meta::info fn_refl = __reflect(query_get_begin_member, reflexpr(fn));
  {
    constexpr meta::info first_param_refl = __reflect(query_get_begin_param, fn_refl);
    static_assert(string_eq(name_of(first_param_refl), "a"));

    constexpr meta::info second_param_refl = __reflect(query_get_next_param, first_param_refl);
    static_assert(string_eq(name_of(second_param_refl), "b"));

    constexpr meta::info end = __reflect(query_get_next_param, second_param_refl);
    static_assert(is_invalid(end));
  }

  // Legacy
  {
    constexpr meta::info first_param_refl = __reflect(query_get_begin, fn_refl);
    static_assert(string_eq(name_of(first_param_refl), "a"));

    constexpr meta::info second_param_refl = __reflect(query_get_next, first_param_refl);
    static_assert(string_eq(name_of(second_param_refl), "b"));

    constexpr meta::info end = __reflect(query_get_next, second_param_refl);
    static_assert(is_invalid(end));
  }
}

namespace templ_fn {
  template<typename templ_param_a, typename templ_param_b>
  void foo(int a, int b) {
  }
}

constexpr void templ_fn_range_test() {
  constexpr meta::info templ_fn_refl = __reflect(query_get_begin_member, reflexpr(templ_fn));
  {
    constexpr meta::info first_param_refl = __reflect(query_get_begin_param, templ_fn_refl);
    static_assert(string_eq(name_of(first_param_refl), "a"));

    constexpr meta::info second_param_refl = __reflect(query_get_next_param, first_param_refl);
    static_assert(string_eq(name_of(second_param_refl), "b"));

    constexpr meta::info end = __reflect(query_get_next_param, second_param_refl);
    static_assert(is_invalid(end));
  }
  {
    constexpr meta::info first_templ_param_refl = __reflect(query_get_begin_template_param, templ_fn_refl);
    static_assert(string_eq(name_of(first_templ_param_refl), "templ_param_a"));

    constexpr meta::info second_templ_param_refl = __reflect(query_get_next_template_param, first_templ_param_refl);
    static_assert(string_eq(name_of(second_templ_param_refl), "templ_param_b"));

    constexpr meta::info end = __reflect(query_get_next_template_param, second_templ_param_refl);
    static_assert(is_invalid(end));
  }

  // Legacy
  {
    constexpr meta::info first_param_refl = __reflect(query_get_begin, templ_fn_refl);
    static_assert(string_eq(name_of(first_param_refl), "a"));

    constexpr meta::info second_param_refl = __reflect(query_get_next, first_param_refl);
    static_assert(string_eq(name_of(second_param_refl), "b"));

    constexpr meta::info end = __reflect(query_get_next, second_param_refl);
    static_assert(is_invalid(end));
  }
}
