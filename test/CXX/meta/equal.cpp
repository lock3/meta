// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "reflection_query.h"

#define assert(E) if (!(E)) __builtin_abort();

namespace meta {

using info = decltype(reflexpr(void));

consteval bool is_invalid(info m) {
  return __reflect(query_is_invalid, m);
}

consteval bool is_data_member(info m) {
  if (__reflect(query_is_nonstatic_data_member, m))
    return true;

  if (__reflect(query_is_static_data_member, m))
    return true;

  return false;
}

consteval info front(info x) {
  return __reflect(query_get_begin, x);
}

consteval info next(info x) {
  return __reflect(query_get_next, x);
}

} // end namespace meta

struct S {
  int a, b, c;
};

template<meta::info X, typename T>
bool compare(const T& a, const T& b) {
  if constexpr (!meta::is_invalid(X)) {
    if constexpr (meta::is_data_member(X)) {
      auto p = valueof(X);
      if (a.*p != b.*p)
        return false;
    }
    return compare<meta::next(X)>(a, b);
  }
  return true;
}

template<typename T>
bool equal(const T& a, const T& b) {
  return compare<meta::front(reflexpr(T))>(a, b);
}

int main() {
  S s1 { 0, 0, 0 };
  S s2 { 0, 0, 1 };
  assert(equal(s1, s1));
  assert(!equal(s1, s2));
}
