// RUN: %clang_cc1 -freflection -std=c++2a %s

#include "reflection_iterator.h"

consteval char const* name_of(meta::info x) {
  return __reflect(query_get_name, x);
}

enum E { A, B, C };

struct S {
  int a = 0, b = 1, c = 2;
};

extern "C" int puts(char const* str);

template<typename T> // requires Enum<T>
char const* to_string(T val) {
  static constexpr auto range = meta::range(reflexpr(T));
  template for (constexpr meta::info member : range) {
    if (valueof(member) == val)
      return name_of(member);
  }
  return "<unknown>";
}


int main() {
  puts(name_of(reflexpr(S)));
  puts(name_of(reflexpr(S::a)));

  puts(to_string<E>(A));

  return 0;
}
