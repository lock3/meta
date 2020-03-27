#ifndef REFLECTION_ITERATOR_H
#define REFLECTION_ITERATOR_H

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

template <class T>
constexpr auto begin(T& c) -> decltype(c.begin()) {
  return c.begin();
}

template <class T>
constexpr auto begin(const T& c) -> decltype(c.begin()) {
  return c.begin();
}

template <class T>
constexpr auto end(T& c) -> decltype(c.end()) {
  return c.end();
}

template <class T>
constexpr auto end(const T& c) -> decltype(c.end()) {
  return c.end();
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

namespace meta {
  struct iterator {
    consteval iterator()
      : m_info()
    { }

    consteval iterator(meta::info x)
      : m_info(__reflect(query_get_begin, x))
    { }

    consteval meta::info operator*() const {
      return m_info;
    }

    consteval iterator operator++() {
      m_info = __reflect(query_get_next, m_info);
      return *this;
    }

    consteval iterator operator++(int) {
      iterator tmp = *this;
      operator++();
      return tmp;
    }

    consteval friend bool operator==(iterator a, iterator b) {
      return a.m_info == b.m_info;
    }

    consteval friend bool operator!=(iterator a, iterator b) {
      return a.m_info != b.m_info;
    }

    meta::info m_info;
  };

  struct range {
    consteval range() { }

    consteval range(meta::info cxt)
      : m_first(cxt), m_last()
    { }

    consteval iterator begin() const { return m_first; }

    consteval iterator end() const { return m_last; }

    iterator m_first;
    iterator m_last;
  };

  consteval iterator begin(info x) {
    return iterator(x);
  }

  consteval iterator end(info x) {
    return iterator();
  }
}

#endif
