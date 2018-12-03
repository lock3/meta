// RUN: %clang_cc1 -std=c++1z -freflection %s

#include "reflection_query.h"

namespace meta {

using info = decltype(reflexpr(void));

consteval info next(info x) {
  return __reflect(query_get_next, x);
}

class iterator {
  meta::info m_info;

public:
  constexpr iterator()
    : m_info()
  { }

  constexpr iterator(meta::info x)
    : m_info(x)
  { }

  constexpr info operator*() const {
    return m_info;
  }

  constexpr iterator operator++() {
    m_info = next(m_info);
    return *this;
  }

  constexpr iterator operator++(int) {
    iterator tmp = *this;
    operator++();
    return tmp;
  }

  constexpr friend bool operator==(iterator a, iterator b) {
    return a.m_info == b.m_info;
  }

  constexpr friend bool operator!=(iterator a, iterator b) {
    return a.m_info != b.m_info;
  }
};

consteval iterator begin(info x) {
  return __reflect(query_get_begin, x);
}

consteval iterator end(info x) {
  return iterator();
}

} // end namespace meta

namespace N {
  void f1();
  void f2();
  void f3();
  void f4();
}

constexpr int count_members(meta::info x) {
  meta::iterator first = meta::begin(x);
  meta::iterator last = meta::end(x);
  int n = 0;
  while (first != last) {
    ++first;
    ++n;
  }
  return n;
}

int main(int argc, char* argv[]) {
  static_assert(count_members(reflexpr(N)) == 4);
}
