// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

class Foo {
  static void a() { }
  void b() { }
  virtual void c() { }
  virtual void d() = 0;
  void e() = delete;
};

namespace foo_base {
  constexpr meta::info method_1 = __reflect(query_get_begin, reflexpr(Foo));
  static_assert(__reflect(query_is_static_member_function, method_1));
  static_assert(!__reflect(query_is_nonstatic_member_function, method_1));
  static_assert(__reflect(query_is_normal, method_1));
  static_assert(!__reflect(query_is_override, method_1));
  static_assert(!__reflect(query_is_override_specified, method_1));
  static_assert(!__reflect(query_is_deleted, method_1));
  static_assert(!__reflect(query_is_virtual, method_1));
  static_assert(!__reflect(query_is_pure_virtual, method_1));

  constexpr meta::info method_2 = __reflect(query_get_next, method_1);
  static_assert(!__reflect(query_is_static_member_function, method_2));
  static_assert(__reflect(query_is_nonstatic_member_function, method_2));
  static_assert(__reflect(query_is_normal, method_2));
  static_assert(!__reflect(query_is_override, method_2));
  static_assert(!__reflect(query_is_override_specified, method_2));
  static_assert(!__reflect(query_is_deleted, method_2));
  static_assert(!__reflect(query_is_virtual, method_2));
  static_assert(!__reflect(query_is_pure_virtual, method_2));

  constexpr meta::info method_3 = __reflect(query_get_next, method_2);
  static_assert(!__reflect(query_is_static_member_function, method_3));
  static_assert(__reflect(query_is_nonstatic_member_function, method_3));
  static_assert(__reflect(query_is_normal, method_3));
  static_assert(!__reflect(query_is_override, method_3));
  static_assert(!__reflect(query_is_override_specified, method_3));
  static_assert(!__reflect(query_is_deleted, method_3));
  static_assert(__reflect(query_is_virtual, method_3));
  static_assert(!__reflect(query_is_pure_virtual, method_3));

  constexpr meta::info method_4 = __reflect(query_get_next, method_3);
  static_assert(!__reflect(query_is_static_member_function, method_4));
  static_assert(__reflect(query_is_nonstatic_member_function, method_4));
  static_assert(__reflect(query_is_normal, method_4));
  static_assert(!__reflect(query_is_override, method_4));
  static_assert(!__reflect(query_is_override_specified, method_4));
  static_assert(!__reflect(query_is_deleted, method_4));
  static_assert(__reflect(query_is_virtual, method_4));
  static_assert(__reflect(query_is_pure_virtual, method_4));

  constexpr meta::info method_5 = __reflect(query_get_next, method_4);
  static_assert(!__reflect(query_is_static_member_function, method_5));
  static_assert(__reflect(query_is_nonstatic_member_function, method_5));
  static_assert(__reflect(query_is_normal, method_5));
  static_assert(!__reflect(query_is_override, method_5));
  static_assert(!__reflect(query_is_override_specified, method_5));
  static_assert(__reflect(query_is_deleted, method_5));
  static_assert(!__reflect(query_is_virtual, method_5));
  static_assert(!__reflect(query_is_pure_virtual, method_5));
}

class FooChildImplicit : Foo {
  void c() { }
  void d() { }
};

namespace foo_child_implicit {
  constexpr meta::info method_1 = __reflect(query_get_begin, reflexpr(FooChildImplicit));
  static_assert(!__reflect(query_is_static_member_function, method_1));
  static_assert(__reflect(query_is_nonstatic_member_function, method_1));
  static_assert(__reflect(query_is_normal, method_1));
  static_assert(__reflect(query_is_override, method_1));
  static_assert(!__reflect(query_is_override_specified, method_1));
  static_assert(!__reflect(query_is_deleted, method_1));
  static_assert(__reflect(query_is_virtual, method_1));
  static_assert(!__reflect(query_is_pure_virtual, method_1));

  constexpr meta::info method_2 = __reflect(query_get_next, method_1);
  static_assert(!__reflect(query_is_static_member_function, method_2));
  static_assert(__reflect(query_is_nonstatic_member_function, method_2));
  static_assert(__reflect(query_is_normal, method_2));
  static_assert(__reflect(query_is_override, method_2));
  static_assert(!__reflect(query_is_override_specified, method_2));
  static_assert(!__reflect(query_is_deleted, method_2));
  static_assert(__reflect(query_is_virtual, method_2));
  static_assert(!__reflect(query_is_pure_virtual, method_2));
}

class FooChildExplicit : Foo {
  void c() override { }
  void d() override { }
};

namespace foo_child_explicit {
  constexpr meta::info method_1 = __reflect(query_get_begin, reflexpr(FooChildExplicit));
  static_assert(!__reflect(query_is_static_member_function, method_1));
  static_assert(__reflect(query_is_nonstatic_member_function, method_1));
  static_assert(__reflect(query_is_normal, method_1));
  static_assert(__reflect(query_is_override, method_1));
  static_assert(__reflect(query_is_override_specified, method_1));
  static_assert(!__reflect(query_is_deleted, method_1));
  static_assert(__reflect(query_is_virtual, method_1));
  static_assert(!__reflect(query_is_pure_virtual, method_1));

  constexpr meta::info method_2 = __reflect(query_get_next, method_1);
  static_assert(!__reflect(query_is_static_member_function, method_2));
  static_assert(__reflect(query_is_nonstatic_member_function, method_2));
  static_assert(__reflect(query_is_normal, method_2));
  static_assert(__reflect(query_is_override, method_2));
  static_assert(__reflect(query_is_override_specified, method_2));
  static_assert(!__reflect(query_is_deleted, method_2));
  static_assert(__reflect(query_is_virtual, method_2));
  static_assert(!__reflect(query_is_pure_virtual, method_2));
}
