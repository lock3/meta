// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../../support/query.h"

namespace single_match {

template<typename T>
void fn() { }

constexpr auto specialization = __reflect(query_get_begin_overload_candidate, ^fn<int>);
static_assert(__reflect(query_is_specialization, specialization));

constexpr auto invalid_specialization = __reflect(query_get_next_overload_candidate, specialization);
static_assert(__reflect(query_is_invalid, invalid_specialization));

}

namespace multi_match {

namespace hidden {
  struct bar { };

  template<typename T>
  void fn(T x) { }
}

void fn() { }

template<typename T>
void fn(T a) { }

template<typename T>
void fn() { }

constexpr auto specialization = __reflect(query_get_begin_overload_candidate, ^fn<int>);
static_assert(__reflect(query_is_specialization, specialization));

constexpr auto next_specialization = __reflect(query_get_next_overload_candidate, specialization);
static_assert(__reflect(query_is_specialization, next_specialization));

constexpr auto invalid_specialization = __reflect(query_get_next_overload_candidate, next_specialization);
static_assert(__reflect(query_is_invalid, invalid_specialization));

}

namespace sfinae {

template<int I>
void fn(char(*)[I % 2 == 0] = 0) { }

template<int I>
void fn(char(*)[I % 2 == 1] = 1) { }

constexpr auto specialization = __reflect(query_get_begin_overload_candidate, ^fn<1>);
static_assert(__reflect(query_is_specialization, specialization));

constexpr auto next_specialization = __reflect(query_get_next_overload_candidate, specialization);
static_assert(__reflect(query_is_specialization, next_specialization));

constexpr auto invalid_specialization = __reflect(query_get_next_overload_candidate, next_specialization);
static_assert(__reflect(query_is_invalid, invalid_specialization));

}
