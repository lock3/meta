// RUN: %clang_cc1 -fcxx-exceptions -fsyntax-only -Wlifetime -Wlifetime-debug -verify %s
#include "../Analysis/Inputs/system-header-simulator-cxx.h"
template <typename T>
bool __lifetime_pset(const T &) { return true; }

template <typename T>
bool __lifetime_pset_ref(const T &) { return true; }

template <typename T>
void __lifetime_type_category() {}

namespace gsl {
// All have different values.
// TODO: these prohibit using initializer_list<T>.. probably need tuples.
struct null_t {
  template<typename T>
  operator T() const { return T(nullptr); }
} Null;
struct static_t {
  template <typename T>
  operator T() const { return (T)(void*)this; } // TODO: get rid of this warning.
} Static;
struct invalid_t {
  template <typename T>
  operator T() const { return (T)(void*)this; } // TODO: get rid of this warning.
} Invalid;

template <typename T>
struct CheckSingle {
  CheckSingle(const T &t) : data(t) {}
  const T &data;
  template<typename S>
  operator CheckSingle<S> () { return CheckSingle<S>(S(data)); }
};

template <typename T>
struct CheckVariadic {
  CheckVariadic(std::initializer_list<T> ptrs) : ptrs(ptrs) {}
  // We expect this to live only for a single expr.
  std::initializer_list<T> ptrs;
};

template <typename T, typename S>
bool operator==(CheckSingle<T> lhs, CheckSingle<S> rhs) {
  // TODO: these cannot be checked right?
  if ((void *)lhs.data == (void *)&Static || (void *)lhs.data == (void *)&Invalid ||
      (void *)rhs.data == (void *)&Static || (void *)rhs.data == (void *)&Invalid)
    return true;
  // TODO: maybe make this a customization point?
  //       user defined gsl::Pointers might not have operator==.
  //       Alternative: fall back to &deref(UserPtr).
  return lhs.data == rhs.data;
}

template<typename T, typename S>
bool operator==(const CheckVariadic<T>& lhs, CheckSingle<S> rhs) {
  return std::any_of(lhs.ptrs.begin(), lhs.ptrs.end(), [&rhs](const T &ptr) {
    return CheckSingle<T>(ptr) == rhs;
  });
}

template<typename T, typename S>
bool operator==(const CheckSingle<T>& lhs, CheckVariadic<S> rhs) {
  return rhs == lhs; 
}

template<typename T>
CheckSingle<T> pset(const T &t) {
  return t;
}

template<typename T>
CheckVariadic<T> pset(std::initializer_list<T> ptrs) {
  return CheckVariadic<T>(ptrs);
}

// TODO: support deref
// TODO: support member selection (change in Attr representation)
// TODO: handle references (auto deref?)
} // namespace gsl

using namespace gsl;

void basic(int *a, int *b) [[gsl::pre(pset(b) == pset(a))]] {
  __lifetime_pset(b); // expected-warning {{((*a))}}
}

void specials(int *a, int *b, int *c)
    [[gsl::pre(pset(a) == pset(Null))]]
    [[gsl::pre(pset(b) == pset(Static))]]
    [[gsl::pre(pset(c) == pset(Invalid))]] {
  __lifetime_pset(a); // expected-warning {{((null))}}
  __lifetime_pset(b); // expected-warning {{((static))}}
  __lifetime_pset(c); // expected-warning {{((invalid))}}
}

void variadic(int *a, int *b, int *c)
    [[gsl::pre(pset(b) == pset({a, c}))]] {
  __lifetime_pset(b); // expected-warning {{((*a), (*c))}}
}

// TODO: swapped variadic

/* Will not compile! What should this mean for the state of the analysis?
   The source of the problem is that the following constraint can 
   be satisfied multiple ways:
   pset(a, b) == pset(c, d)
   Possible solution #1:
    pset(a) == {*a}
    pset(b) == {*a, b}
    pset(c) == {*a}
    pset(d) == {*b}
   Possible solution #2:
    pset(a) == {*a}
    pset(b) == {*a}
    pset(c) == {*a}
    pset(d) == {*a}
   And so on...
void double_variadic(int *a, int *b, int *c)
    [[gsl::pre(pset({a, b}) == pset({b, c}))]] {
}
*/

void multiple_annotations(int *a, int *b, int *c)
    [[gsl::pre(pset(b) == pset(a))]]
    [[gsl::pre(pset(c) == pset(a))]] {
  __lifetime_pset(b); // expected-warning {{((*a))}}
  __lifetime_pset(c); // expected-warning {{((*a))}}
}

void multiple_annotations_chained(int *a, int *b, int *c)
    [[gsl::pre(pset(b) == pset(a))]]
    [[gsl::pre(pset(c) == pset(b))]] {
  __lifetime_pset(b); // expected-warning {{((*a))}}
  //__lifetime_pset(c); // TODOexpected-warning {{((*a))}}
}