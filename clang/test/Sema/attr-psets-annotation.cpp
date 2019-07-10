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
void * const Null = nullptr;
void * const Static = (void *)&Null;
void * const Invalid = (void *)&Static; 

template <typename T>
struct CheckSingle {
  CheckSingle(const T &t) : data(t) {}
  const T &data;
};

template <typename T>
struct CheckVariadic {
  CheckVariadic(std::initializer_list<T> ptrs) : ptrs(ptrs) {}
  // We expect this to live only for a single expr.
  std::initializer_list<T> ptrs;
};

template<typename T>
bool operator==(CheckSingle<T> lhs, CheckSingle<T> rhs) {
  // TODO: these cannot be checked right?
  if ((void *)lhs.data == Static || (void*)lhs.data == Invalid ||
      (void *)rhs.data == Static || (void*)rhs.data == Invalid)
      return true;
  if (lhs.data == rhs.data)
    return true;
  return false;
}

template<typename T>
bool operator==(const CheckVariadic<T>& lhs, CheckSingle<T> rhs) {
  return std::any_of(lhs.ptrs.begin(), lhs.ptrs.end(), [&rhs](const T &ptr) {
    return CheckSingle<T>(ptr) == rhs;
  });
}

template<typename T>
bool operator==(const CheckSingle<T>& lhs, CheckVariadic<T> rhs) {
  return rhs == lhs; 
}

template<typename T>
CheckSingle<T> pset(T t) {
  return t;
}

template<typename T>
CheckVariadic<T> pset(std::initializer_list<T> ptrs) {
  return CheckVariadic<T>(ptrs);
}
}

using namespace gsl;

void basic(int *a, int *b) [[gsl::pre(pset(b) == pset(a))]] {
  __lifetime_pset(b); // expected-warning {{((*a))}}
}

void variadic(int *a, int *b, int *c)
    [[gsl::pre(pset(b) == pset({a, c}))]] {
  __lifetime_pset(b); // expected-warning {{((*a), (*c))}}
}

/* Will not compile!
void double_variadic(int *a, int *b, int *c)
    [[gsl::pre(pset({a, b}) == pset({b, c}))]] {
}
*/