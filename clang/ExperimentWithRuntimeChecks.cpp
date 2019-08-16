// RUN: %clang_cc1 -fcxx-exceptions -fsyntax-only -Wlifetime -Wlifetime-debug -verify %s
#include <initializer_list>
#include <algorithm>
#include <tuple>
#include <utility>
#include <iostream>
template <typename T>
bool __lifetime_pset(const T &) { return true; }

template <typename T>
bool __lifetime_pset_ref(const T &) { return true; }

template <typename T>
void __lifetime_type_category() {}

#define assert(expr) \
    if (!(expr)) \
        std::cout << "Assert fail: " <<  #expr << std::endl;

namespace gsl {
template <typename T>
using nullable = T;

template <typename T>
struct not_null {
  constexpr operator T() const { return T(); }
  constexpr T operator->() const { return T(); }
};

struct null_t {
  int &operator*() const;
  operator void *() const { return nullptr; }
} Null;
struct static_t {
  int &operator*() const;
} Static;
struct invalid_t {
  int &operator*() const;
} Invalid;
struct return_t {
  operator void *() const { return (void *)this; }
  // TODO: eliminate the methods below.
  template <typename T>
  return_t(const T&) {}
  return_t() {}
  bool operator==(return_t) const { return true; }
} Return;

std::ostream &operator<<(std::ostream &os, static_t) {
  return os << "static";
}
std::ostream &operator<<(std::ostream &os, invalid_t) {
  return os << "invalid";
}

template <typename T>
struct PointerTraits {
  static auto deref(const T &t) -> decltype(*t) { return *t; }
  static const bool isNull(const T &t) { return false; }
  static const bool checked = false;
};

template <typename T>
struct PointerTraits<T *> {
  static const T &deref(const T *t) { return *t; }
  static const bool isNull(const T *t) { return !t; }
  static const bool checked = true;
};

template <typename T>
struct PointerTraits<T &> {
  static const T &deref(const T &t) { return t; }
  static const bool isNull(const T &t) { return false; }
  static const bool checked = true;
};

template <typename T>
struct CheckSingle {
  CheckSingle(const T &t) : data(t) {}
  CheckSingle(void *p) : data((T)p) {}
  T data;
  template <typename S>
  operator CheckSingle<S>() { return CheckSingle<S>(S(data)); }
};

template <typename T>
struct [[deprecated]] print_type {};

template <typename T>
auto deref(const T &t) -> decltype(PointerTraits<T>::deref(t)) {
  return PointerTraits<T>::deref(t);
}

// Hack to deduce reference types correclty.
#define deref(arg) deref<decltype(arg)>(arg)

template <typename T, typename S>
bool operator==(CheckSingle<T> lhs, CheckSingle<S> rhs) {
  if (!PointerTraits<decltype(lhs.data)>::checked || !PointerTraits<decltype(rhs.data)>::checked)
    return true;
  // TODO: these cannot be checked right?
  if ((void *)lhs.data == (void *)&Return ||
      std::is_same<decltype(rhs.data), static_t>::value || std::is_same<decltype(rhs.data), invalid_t>::value)
    return true;
  if (PointerTraits<decltype(lhs.data)>::isNull(lhs.data))
    return PointerTraits<decltype(rhs.data)>::isNull(rhs.data);
  if (PointerTraits<decltype(rhs.data)>::isNull(rhs.data))
    return false;
  // User defined pointers might not have operator ==, convert them to raw pointers before checking.
  // TODO: does this work well with references?
  return &deref(lhs.data) == &deref(rhs.data);
}

template<typename T>
struct lifetime_t;

template<std::size_t... Ints>
struct lifetime_t<std::index_sequence<Ints...>> {
  template <typename T, typename... RhsTs>
  static bool impl(const T &lhs, const std::tuple<RhsTs...> &rhs) {
    CheckSingle<T> lhsToCheck(lhs);
    return (... || (lhsToCheck == CheckSingle<RhsTs>(std::get<Ints>(rhs))));
  }
};

template <typename T, typename... RhsTs>
bool lifetime(const T &lhs, const std::tuple<RhsTs...> &rhs) {
  return lifetime_t<std::make_index_sequence<sizeof...(RhsTs)>>::impl(lhs, rhs);
}

bool pre(bool b) { return b; }

} // namespace gsl

using namespace gsl;

void basic(int *a, int *b) [[gsl::pre(lifetime(b, std::tuple{a}))]] {
  assert(gsl::pre(lifetime(b, std::tuple{a})));
}

void basic_ref(int *a, int &b) [[gsl::pre(lifetime(&b, std::tuple{a}))]] {
  assert(gsl::pre(lifetime(&b, std::tuple{a})));
}

void specials(int *a, int *b, int *c)
    [[gsl::pre(lifetime(a, std::tuple{Null}))]]
    [[gsl::pre(lifetime(b, std::tuple{Static}))]]
    [[gsl::pre(lifetime(c, std::tuple{Invalid}))]] {

  assert(gsl::pre(lifetime(a, std::tuple{Null})));
  assert(gsl::pre(lifetime(b, std::tuple{Static})));
  assert(gsl::pre(lifetime(c, std::tuple{Invalid})));
}

void variadic(int *a, int *b, int *c)
    [[gsl::pre(lifetime(b, std::tuple{a, c}))]] {
  assert(gsl::pre(lifetime(b, std::tuple{a, c})));
}

int main() {
  int a = 1, b = 2, c = 3;
  basic(&a, &a);
  basic(&a, &b);
  specials(nullptr, &a, &b);
  specials(&c, &a, &b);
  variadic(&a, &b, &c);
  variadic(&a, &a, &c);
  variadic(&a, &c, &c);
  basic_ref(&a, b);
}