// RUN: %clang_cc1 -freflection -std=c++1z %s
// expected-no-diagnostics

#include "reflection_query.h"

namespace std {

template<typename I>
constexpr int distance(I first, I last) {
  int n = 0;
  while (first != last) {
    ++first;
    ++n;
  }
  return n;
}

template<typename I>
constexpr I next(I iter, int advancement) {
  for (int i = 0; i < advancement; ++i)
    ++iter;
  return iter;
}

template<class ...TupleValType>
class tuple {
};

} // namespace std

namespace meta {
  using info = decltype(reflexpr(void));
}

// Dummy to satisfy lookup requirements of
// expansion statements.
template<int Index, class ...TupleValType>
int get(std::tuple<TupleValType...>& t) {
  return 0;
}

struct member_iterator
{
  constexpr member_iterator()
    : m_info()
  { }

  constexpr member_iterator(meta::info x)
    : m_info(__reflect(query_get_begin, x))
  { }

  constexpr meta::info operator*() const {
    return m_info;
  }

  constexpr member_iterator operator++() {
    m_info = __reflect(query_get_next, m_info);
    return *this;
  }

  constexpr member_iterator operator++(int) {
    member_iterator tmp = *this;
    operator++();
    return tmp;
  }

  constexpr friend bool operator==(member_iterator a, member_iterator b) {
    return a.m_info == b.m_info;
  }

  constexpr friend bool operator!=(member_iterator a, member_iterator b) {
    return a.m_info != b.m_info;
  }

  meta::info m_info;
};

struct member_range
{
  constexpr member_range() { }

  constexpr member_range(meta::info cxt)
    : m_first(cxt), m_last()
  { }

  constexpr member_iterator begin() const { return m_first; }

  constexpr member_iterator end() const { return m_last; }

  member_iterator m_first;
  member_iterator m_last;
};


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
  static constexpr auto range = member_range(reflexpr(T));
  for... (constexpr meta::info member : range) {
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
