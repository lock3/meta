// RUN: %clang_cc1 -freflection -std=c++2a %s

#include "../reflection_query.h"

namespace existing_ns {

void fn_1() { }
void fn_2() { }

int var_1;
int var_2;

namespace ns_1 {
  void fn_1() { }
}

namespace ns_2 {
  void fn_1() { }
}

class class_1 {
  static int static_var_1;
  static int static_var_2;
};

}

namespace new_ns {

consteval {
  // Functions
  auto fn_1 = __reflect(query_get_begin_member, reflexpr(existing_ns));
  __reflect_mod(query_set_add_inline, fn_1, true);

  -> fn_1;

  auto fn_2 = __reflect(query_get_next_member, fn_1);
  __reflect_mod(query_set_add_inline, fn_2, true);
  __reflect_mod(query_set_add_inline, fn_2, false);

  -> fn_2;

  // Variables
  auto var_1 = __reflect(query_get_next_member, fn_2);
  __reflect_mod(query_set_add_inline, var_1, true);

  -> var_1;

  auto var_2 = __reflect(query_get_next_member, var_1);
  __reflect_mod(query_set_add_inline, var_2, true);
  __reflect_mod(query_set_add_inline, var_2, false);

  -> var_2;

  // Namespaces
  auto ns_1 = __reflect(query_get_next_member, var_2);
  __reflect_mod(query_set_add_inline, ns_1, true);

  -> ns_1;

  auto ns_2 = __reflect(query_get_next_member, ns_1);
  __reflect_mod(query_set_add_inline, ns_2, true);
  __reflect_mod(query_set_add_inline, ns_2, false);

  -> ns_2;
}

class class_1 {
  consteval {
    // Static Data Members
    auto class_1 = reflexpr(existing_ns::class_1);
    auto static_var_1 = __reflect(query_get_begin_member, class_1);
    __reflect_mod(query_set_add_inline, static_var_1, true);

    -> static_var_1;

    auto static_var_2 = __reflect(query_get_next_member, static_var_1);
    __reflect_mod(query_set_add_inline, static_var_2, true);
    __reflect_mod(query_set_add_inline, static_var_2, false);

    -> static_var_2;
  }
};

}

// Functions
constexpr auto fn_1 = __reflect(query_get_begin_member, reflexpr(new_ns));
static_assert(__reflect(query_is_inline, fn_1));

constexpr auto fn_2 = __reflect(query_get_next_member, fn_1);
static_assert(!__reflect(query_is_inline, fn_2));

// Variables
constexpr auto var_1 = __reflect(query_get_next_member, fn_2);
static_assert(__reflect(query_is_inline, var_1));

constexpr auto var_2 = __reflect(query_get_next_member, var_1);
static_assert(!__reflect(query_is_inline, var_2));

// Namespaces
constexpr auto ns_1 = __reflect(query_get_next_member, var_2);
static_assert(__reflect(query_is_inline, ns_1));

constexpr auto ns_2 = __reflect(query_get_next_member, ns_1);
static_assert(!__reflect(query_is_inline, ns_2));

// Static Data Members
constexpr auto class_1 = __reflect(query_get_next_member, ns_2);
constexpr auto static_var_1 = __reflect(query_get_begin_member, class_1);
static_assert(__reflect(query_is_inline, static_var_1));

constexpr auto static_var_2 = __reflect(query_get_next_member, static_var_1);
static_assert(!__reflect(query_is_inline, static_var_2));

int main () {
  return 0;
}
