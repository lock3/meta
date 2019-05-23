// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

class C {
  int a;
private:
  int b;
protected:
  int c;
public:
  int d;
};

constexpr meta::info member_1 = __reflect(query_get_begin, reflexpr(C));
static_assert(__reflect(query_has_access, member_1));
static_assert(!__reflect(query_is_public, member_1));
static_assert(!__reflect(query_is_protected, member_1));
static_assert(__reflect(query_is_private, member_1));
static_assert(__reflect(query_has_default_access, member_1));

constexpr meta::info member_2 = __reflect(query_get_next, member_1);
static_assert(__reflect(query_has_access, member_2));
static_assert(!__reflect(query_is_public, member_2));
static_assert(!__reflect(query_is_protected, member_2));
static_assert(__reflect(query_is_private, member_2));
static_assert(!__reflect(query_has_default_access, member_2));

constexpr meta::info member_3 = __reflect(query_get_next, member_2);
static_assert(__reflect(query_has_access, member_3));
static_assert(!__reflect(query_is_public, member_3));
static_assert(__reflect(query_is_protected, member_3));
static_assert(!__reflect(query_is_private, member_3));
static_assert(!__reflect(query_has_default_access, member_3));

constexpr meta::info member_4 = __reflect(query_get_next, member_3);
static_assert(__reflect(query_has_access, member_4));
static_assert(__reflect(query_is_public, member_4));
static_assert(!__reflect(query_is_protected, member_4));
static_assert(!__reflect(query_is_private, member_4));
static_assert(!__reflect(query_has_default_access, member_4));

int x = 0;

constexpr meta::info global_var = reflexpr(x);
static_assert(!__reflect(query_has_access, global_var));
static_assert(!__reflect(query_is_public, global_var));
static_assert(!__reflect(query_is_protected, global_var));
static_assert(!__reflect(query_is_private, global_var));
static_assert(!__reflect(query_has_default_access, global_var));
