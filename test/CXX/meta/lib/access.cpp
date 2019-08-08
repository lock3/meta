// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

class C {
  int a_1;
  void a_2() { }
  using a_3 = int;
private:
  int b_1;
  void b_2() { }
  using b_3 = int;
protected:
  int c_1;
  void c_2() { }
  using c_3 = int;
public:
  int d_1;
  void d_2() { }
  using d_3 = int;
};

namespace default_access {
  constexpr meta::info member_1 = __reflect(query_get_begin_member, reflexpr(C));
  static_assert(__reflect(query_has_access, member_1));
  static_assert(!__reflect(query_is_public, member_1));
  static_assert(!__reflect(query_is_protected, member_1));
  static_assert(__reflect(query_is_private, member_1));
  static_assert(__reflect(query_has_default_access, member_1));

  constexpr meta::info member_2 = __reflect(query_get_next_member, member_1);
  static_assert(__reflect(query_has_access, member_2));
  static_assert(!__reflect(query_is_public, member_2));
  static_assert(!__reflect(query_is_protected, member_2));
  static_assert(__reflect(query_is_private, member_2));
  static_assert(__reflect(query_has_default_access, member_2));

  constexpr meta::info member_3 = __reflect(query_get_next_member, member_2);
  static_assert(__reflect(query_has_access, member_3));
  static_assert(!__reflect(query_is_public, member_3));
  static_assert(!__reflect(query_is_protected, member_3));
  static_assert(__reflect(query_is_private, member_3));
  static_assert(__reflect(query_has_default_access, member_3));
}

namespace private_access {
  constexpr meta::info member_1 = __reflect(query_get_next_member, default_access::member_3);
  static_assert(__reflect(query_has_access, member_1));
  static_assert(!__reflect(query_is_public, member_1));
  static_assert(!__reflect(query_is_protected, member_1));
  static_assert(__reflect(query_is_private, member_1));
  static_assert(!__reflect(query_has_default_access, member_1));

  constexpr meta::info member_2 = __reflect(query_get_next_member, member_1);
  static_assert(__reflect(query_has_access, member_2));
  static_assert(!__reflect(query_is_public, member_2));
  static_assert(!__reflect(query_is_protected, member_2));
  static_assert(__reflect(query_is_private, member_2));
  static_assert(!__reflect(query_has_default_access, member_2));

  constexpr meta::info member_3 = __reflect(query_get_next_member, member_2);
  static_assert(__reflect(query_has_access, member_3));
  static_assert(!__reflect(query_is_public, member_3));
  static_assert(!__reflect(query_is_protected, member_3));
  static_assert(__reflect(query_is_private, member_3));
  static_assert(!__reflect(query_has_default_access, member_3));
}

namespace protected_access {
  constexpr meta::info member_1 = __reflect(query_get_next_member, private_access::member_3);
  static_assert(__reflect(query_has_access, member_1));
  static_assert(!__reflect(query_is_public, member_1));
  static_assert(__reflect(query_is_protected, member_1));
  static_assert(!__reflect(query_is_private, member_1));
  static_assert(!__reflect(query_has_default_access, member_1));

  constexpr meta::info member_2 = __reflect(query_get_next_member, member_1);
  static_assert(__reflect(query_has_access, member_2));
  static_assert(!__reflect(query_is_public, member_2));
  static_assert(__reflect(query_is_protected, member_2));
  static_assert(!__reflect(query_is_private, member_2));
  static_assert(!__reflect(query_has_default_access, member_2));

  constexpr meta::info member_3 = __reflect(query_get_next_member, member_2);
  static_assert(__reflect(query_has_access, member_3));
  static_assert(!__reflect(query_is_public, member_3));
  static_assert(__reflect(query_is_protected, member_3));
  static_assert(!__reflect(query_is_private, member_3));
  static_assert(!__reflect(query_has_default_access, member_3));
}

namespace public_access {
  constexpr meta::info member_1 = __reflect(query_get_next_member, protected_access::member_3);
  static_assert(__reflect(query_has_access, member_1));
  static_assert(__reflect(query_is_public, member_1));
  static_assert(!__reflect(query_is_protected, member_1));
  static_assert(!__reflect(query_is_private, member_1));
  static_assert(!__reflect(query_has_default_access, member_1));

 constexpr meta::info member_2 = __reflect(query_get_next_member, member_1);
  static_assert(__reflect(query_has_access, member_2));
  static_assert(__reflect(query_is_public, member_2));
  static_assert(!__reflect(query_is_protected, member_2));
  static_assert(!__reflect(query_is_private, member_2));
  static_assert(!__reflect(query_has_default_access, member_2));

  constexpr meta::info member_3 = __reflect(query_get_next_member, member_2);
  static_assert(__reflect(query_has_access, member_3));
  static_assert(__reflect(query_is_public, member_3));
  static_assert(!__reflect(query_is_protected, member_3));
  static_assert(!__reflect(query_is_private, member_3));
  static_assert(!__reflect(query_has_default_access, member_3));
}

int x = 0;

constexpr meta::info global_var = reflexpr(x);
static_assert(!__reflect(query_has_access, global_var));
static_assert(!__reflect(query_is_public, global_var));
static_assert(!__reflect(query_is_protected, global_var));
static_assert(!__reflect(query_is_private, global_var));
static_assert(!__reflect(query_has_default_access, global_var));
