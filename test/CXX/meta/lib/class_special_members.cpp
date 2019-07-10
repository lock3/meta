// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

class Foo {
  Foo() = default;
  Foo(const Foo& f) = default;
  Foo(Foo&& f) = default;
  ~Foo() = default;

  Foo &operator=(const Foo& f) = default;
  Foo &operator=(Foo&& f) = default;

  operator int() { return 0; }
};

namespace foo {
  constexpr meta::info member_1 = __reflect(query_get_begin, reflexpr(Foo));
  static_assert(__reflect(query_is_constructor, member_1));
  static_assert(__reflect(query_is_default_constructor, member_1));
  static_assert(!__reflect(query_is_copy_constructor, member_1));
  static_assert(!__reflect(query_is_move_constructor, member_1));
  static_assert(!__reflect(query_is_copy_assignment_operator, member_1));
  static_assert(!__reflect(query_is_move_assignment_operator, member_1));
  static_assert(!__reflect(query_is_destructor, member_1));
  static_assert(!__reflect(query_is_conversion, member_1));
  static_assert(__reflect(query_is_defaulted, member_1));
  static_assert(!__reflect(query_is_explicit, member_1));

  constexpr meta::info member_2 = __reflect(query_get_next, member_1);
  static_assert(__reflect(query_is_constructor, member_2));
  static_assert(!__reflect(query_is_default_constructor, member_2));
  static_assert(__reflect(query_is_copy_constructor, member_2));
  static_assert(!__reflect(query_is_move_constructor, member_2));
  static_assert(!__reflect(query_is_copy_assignment_operator, member_2));
  static_assert(!__reflect(query_is_move_assignment_operator, member_2));
  static_assert(!__reflect(query_is_destructor, member_2));
  static_assert(!__reflect(query_is_conversion, member_2));
  static_assert(__reflect(query_is_defaulted, member_2));
  static_assert(!__reflect(query_is_explicit, member_2));

  constexpr meta::info member_3 = __reflect(query_get_next, member_2);
  static_assert(__reflect(query_is_constructor, member_3));
  static_assert(!__reflect(query_is_default_constructor, member_3));
  static_assert(!__reflect(query_is_copy_constructor, member_3));
  static_assert(__reflect(query_is_move_constructor, member_3));
  static_assert(!__reflect(query_is_copy_assignment_operator, member_3));
  static_assert(!__reflect(query_is_move_assignment_operator, member_3));
  static_assert(!__reflect(query_is_destructor, member_3));
  static_assert(!__reflect(query_is_conversion, member_3));
  static_assert(__reflect(query_is_defaulted, member_3));
  static_assert(!__reflect(query_is_explicit, member_3));

  constexpr meta::info member_4 = __reflect(query_get_next, member_3);
  static_assert(!__reflect(query_is_constructor, member_4));
  static_assert(!__reflect(query_is_default_constructor, member_4));
  static_assert(!__reflect(query_is_copy_constructor, member_4));
  static_assert(!__reflect(query_is_move_constructor, member_4));
  static_assert(!__reflect(query_is_copy_assignment_operator, member_4));
  static_assert(!__reflect(query_is_move_assignment_operator, member_4));
  static_assert(__reflect(query_is_destructor, member_4));
  static_assert(!__reflect(query_is_conversion, member_4));
  static_assert(__reflect(query_is_defaulted, member_4));
  static_assert(!__reflect(query_is_explicit, member_4));

  constexpr meta::info member_5 = __reflect(query_get_next, member_4);
  static_assert(!__reflect(query_is_constructor, member_5));
  static_assert(!__reflect(query_is_default_constructor, member_5));
  static_assert(!__reflect(query_is_copy_constructor, member_5));
  static_assert(!__reflect(query_is_move_constructor, member_5));
  static_assert(__reflect(query_is_copy_assignment_operator, member_5));
  static_assert(!__reflect(query_is_move_assignment_operator, member_5));
  static_assert(!__reflect(query_is_destructor, member_5));
  static_assert(!__reflect(query_is_conversion, member_5));
  static_assert(__reflect(query_is_defaulted, member_5));
  static_assert(!__reflect(query_is_explicit, member_5));

  constexpr meta::info member_6 = __reflect(query_get_next, member_5);
  static_assert(!__reflect(query_is_constructor, member_6));
  static_assert(!__reflect(query_is_default_constructor, member_6));
  static_assert(!__reflect(query_is_copy_constructor, member_6));
  static_assert(!__reflect(query_is_move_constructor, member_6));
  static_assert(!__reflect(query_is_copy_assignment_operator, member_6));
  static_assert(__reflect(query_is_move_assignment_operator, member_6));
  static_assert(!__reflect(query_is_destructor, member_6));
  static_assert(!__reflect(query_is_conversion, member_6));
  static_assert(__reflect(query_is_defaulted, member_6));
  static_assert(!__reflect(query_is_explicit, member_6));

  constexpr meta::info member_7 = __reflect(query_get_next, member_6);
  static_assert(!__reflect(query_is_constructor, member_7));
  static_assert(!__reflect(query_is_default_constructor, member_7));
  static_assert(!__reflect(query_is_copy_constructor, member_7));
  static_assert(!__reflect(query_is_move_constructor, member_7));
  static_assert(!__reflect(query_is_copy_assignment_operator, member_7));
  static_assert(!__reflect(query_is_move_assignment_operator, member_7));
  static_assert(!__reflect(query_is_destructor, member_7));
  static_assert(__reflect(query_is_conversion, member_7));
  static_assert(!__reflect(query_is_defaulted, member_7));
  static_assert(!__reflect(query_is_explicit, member_7));
}

class FooWrapper {
  FooWrapper() { }
  explicit FooWrapper(int i) { }
};

namespace foo_wrapper {
  constexpr meta::info member_1 = __reflect(query_get_begin, reflexpr(FooWrapper));
  static_assert(__reflect(query_is_constructor, member_1));
  static_assert(__reflect(query_is_default_constructor, member_1));
  static_assert(!__reflect(query_is_copy_constructor, member_1));
  static_assert(!__reflect(query_is_move_constructor, member_1));
  static_assert(!__reflect(query_is_copy_assignment_operator, member_1));
  static_assert(!__reflect(query_is_move_assignment_operator, member_1));
  static_assert(!__reflect(query_is_destructor, member_1));
  static_assert(!__reflect(query_is_conversion, member_1));
  static_assert(!__reflect(query_is_defaulted, member_1));
  static_assert(!__reflect(query_is_explicit, member_1));

  constexpr meta::info member_2 = __reflect(query_get_next, member_1);
  static_assert(__reflect(query_is_constructor, member_2));
  static_assert(!__reflect(query_is_default_constructor, member_2));
  static_assert(!__reflect(query_is_copy_constructor, member_2));
  static_assert(!__reflect(query_is_move_constructor, member_2));
  static_assert(!__reflect(query_is_copy_assignment_operator, member_2));
  static_assert(!__reflect(query_is_move_assignment_operator, member_2));
  static_assert(!__reflect(query_is_destructor, member_2));
  static_assert(!__reflect(query_is_conversion, member_2));
  static_assert(!__reflect(query_is_defaulted, member_2));
  static_assert(__reflect(query_is_explicit, member_2));
}
