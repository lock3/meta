// RUN: %clang_cc1 -fcxx-exceptions -fsyntax-only -Wlifetime -Wlifetime-debug -verify %s
#include "../Analysis/Inputs/system-header-simulator-cxx.h"
template <typename T>
bool __lifetime_pset(const T &) { return true; }

template <typename T>
bool __lifetime_pset_ref(const T &) { return true; }

template <typename T>
void __lifetime_type_category() {}

template <typename T>
bool __lifetime_contracts(const T &) { return true; }

struct [[gsl::Pointer]] my_pointer {
  my_pointer();
  my_pointer &operator=(const my_pointer &);
  int &operator*();
};

namespace gsl {
template <typename T>
using nullable = T;

template <typename T>
struct not_null {
  constexpr operator T() const { return T(); }
  constexpr T operator->() const { return T(); }
};

struct null_t {
  int operator*() const;
  template <typename T>
  operator T() const { return T(nullptr); }
} Null;
struct static_t {
  int operator*() const;
  template <typename T>
  operator T() const { return (T)(void *)this; }
} Static;
struct invalid_t {
  int operator*() const;
  template <typename T>
  operator T() const { return (T)(void *)this; }
} Invalid;
struct return_t {
  int operator*() const;
  template <typename T>
  operator T() const { return (T)(void *)this; }
  // TODO: eliminate the methods below.
  template <typename T>
  return_t(const T&) {}
  return_t() {}
  bool operator==(return_t) const { return true; }
} Return;

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
  T data;
  template <typename S>
  operator CheckSingle<S>() { return CheckSingle<S>(S(data)); }
};

template <typename T>
struct CheckVariadic {
  CheckVariadic(std::initializer_list<T> ptrs) : ptrs(ptrs) {}
  // We expect this to live only for a single expr.
  std::initializer_list<T> ptrs;
};

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
  if ((void *)lhs.data == (void *)&Static || (void *)lhs.data == (void *)&Invalid || (void *)lhs.data == (void *)&Return ||
      (void *)rhs.data == (void *)&Static || (void *)rhs.data == (void *)&Invalid || (void *)rhs.data == (void *)&Return)
    return true;
  if (PointerTraits<decltype(lhs.data)>::isNull(lhs.data))
    return PointerTraits<decltype(rhs.data)>::isNull(rhs.data);
  if (PointerTraits<decltype(rhs.data)>::isNull(rhs.data))
    return false;
  // TODO: User defined pointers might not have operator ==, convert them to raw pointers before checking.
  // TODO: does this work well with references?
  return lhs.data == rhs.data;
}

// TODO: requiring both arguments to be the same type is too restrictive, probable a variadic tuple might work better.
template <typename T>
bool lifetime(const T &lhs, const CheckVariadic<T> &rhs) {
  CheckSingle<T> lhsToCheck(lhs);
  return std::any_of(rhs.ptrs.begin(), rhs.ptrs.end(), [&lhsToCheck](const T &ptr) {
    return CheckSingle<T>(ptr) == lhsToCheck;
  });
}

// TODO: support member selection (change in Attr representation)
} // namespace gsl

using namespace gsl;

void basic(int *a, int *b) [[gsl::pre(lifetime(b, {a}))]] {
  __lifetime_pset(b); // expected-warning {{((null), *a)}}
}

// Proper lifetime preconditions can only be checked with annotations.
void check_lifetime_preconditions() {
  int a, b;
  basic(&a, &a);
  basic(&b, &b);
  basic(&a,
        &b); // expected-warning {{passing a pointer as argument with points-to set (b) where points-to set ((null), a) is expected}}
}

void specials(int *a, int *b, int *c)
    [[gsl::pre(lifetime(a, {Null}))]]
    [[gsl::pre(lifetime(b, {Static}))]]
    [[gsl::pre(lifetime(c, {Invalid}))]] {
  __lifetime_pset(a); // expected-warning {{((null))}}
  __lifetime_pset(b); // expected-warning {{((static))}}
  __lifetime_pset(c); // expected-warning {{((invalid))}}
}

void variadic(int *a, int *b, int *c)
    [[gsl::pre(lifetime(b, {a, c}))]] {
  __lifetime_pset(b); // expected-warning {{((null), *a, *c}}
}


void variadic_special(int *a, int *b, int *c)
    [[gsl::pre(lifetime(b, {a, Null}))]] {
  __lifetime_pset(b); // expected-warning {{((null), *a}}
}

void multiple_annotations(int *a, int *b, int *c)
    [[gsl::pre(lifetime(b, {a}))]]
    [[gsl::pre(lifetime(c, {a}))]] {
  __lifetime_pset(b); // expected-warning {{((null), *a)}}
  __lifetime_pset(c); // expected-warning {{((null), *a)}}
}

void multiple_annotations_chained(int *a, int *b, int *c)
    [[gsl::pre(lifetime(b, {a}))]]
    [[gsl::pre(lifetime(c, {b}))]] {
  __lifetime_pset(b); // expected-warning {{((null), *a)}}
  __lifetime_pset(c); // expected-warning {{((null), *a)}}
}

void annotate_forward_decl(int *a, int *b)
    [[gsl::pre(lifetime(b, {a}))]];

void annotate_forward_decl(int *c, int *d) {
  __lifetime_pset(d); // expected-warning {{((null), *c)}}
}

// Repeated annotations on redeclarations are not checked as
// they will automatically be checked with contracts.

namespace dump_contracts {
// Need to have bodies to fill the lifetime attr.
void p(int *a) {}
void p2(int *a, int &b) {}
void p3(int *a, int *&b) { b = 0; }
void parameter_psets(int value,
                     char *const *in,
                     int &int_ref,
                     const int &const_int_ref,
                     std::unique_ptr<int> owner_by_value,
                     const std::unique_ptr<int> &owner_const_ref,
                     std::unique_ptr<int> &owner_ref,
                     my_pointer ptr_by_value,
                     const my_pointer &ptr_const_ref,
                     my_pointer &ptr_ref,
                     my_pointer *ptr_ptr,
                     const my_pointer *ptr_const_ptr) {}
void p4(int *a, int *b, int *&c)
    [[gsl::pre(lifetime(b, {a}))]] { c = 0; }
int *p5(int *a, int *b) { return a; }
int *p6(int *a, int *b)
    [[gsl::post(lifetime(Return, {a}))]] { return a; }
struct S{
  int *f(int * a, int *b, int *&c) { c = 0; return a; }
  S *g(int * a, int *b, int *&c) { c = 0; return this; }
};
void p7(int *a, int *b, int *&c)
    [[gsl::post(lifetime(deref(c), {a}))]] { c = a; }
void p8(int *a, int *b, int **c)
    [[gsl::post(lifetime(deref(c), {a}))]] { if (c) *c = a; }
void p9(int *a, int *b, int *&c)
    [[gsl::lifetime_in(deref(c))]] {}
int* p10() { return 0; }
// TODO: contracts for function pointers?

void f() {
  __lifetime_contracts(p);
  // expected-warning@-1 {{pset(Pre(a)) = ((null), *a)}}
  __lifetime_contracts(p2);
  // expected-warning@-1 {{pset(Pre(a)) = ((null), *a)}}
  // expected-warning@-2 {{pset(Pre(b)) = (*b)}}
  __lifetime_contracts(p3);
  // expected-warning@-1 {{pset(Pre(a)) = ((null), *a)}}
  // expected-warning@-2 {{pset(Pre(b)) = (*b)}}
  // expected-warning@-3 {{pset(Pre(*b)) = ((invalid))}}
  // expected-warning@-4 {{pset(Post(*b)) = ((null), *a)}}
  __lifetime_contracts(parameter_psets);
  // expected-warning@-1 {{pset(Pre(owner_by_value)) = (*owner_by_value)}}
  // expected-warning@-2 {{pset(Pre(owner_ref)) = (*owner_ref)}}
  // expected-warning@-3 {{pset(Pre(*owner_ref)) = (**owner_ref)}}
  // expected-warning@-4 {{pset(Pre(ptr_ref)) = (*ptr_ref)}}
  // expected-warning@-5 {{pset(Pre(*ptr_ref)) = ((invalid))}}
  // expected-warning@-6 {{pset(Pre(ptr_const_ref)) = (*ptr_const_ref)}}
  // expected-warning@-7 {{pset(Pre(*ptr_const_ref)) = ((null), **ptr_const_ref)}}
  // expected-warning@-8 {{pset(Pre(ptr_const_ptr)) = ((null), *ptr_const_ptr)}}
  // expected-warning@-9 {{pset(Pre(*ptr_const_ptr)) = ((null), **ptr_const_ptr)}}
  // expected-warning@-10 {{pset(Pre(in)) = ((null), *in)}}
  // expected-warning@-11 {{pset(Pre(*in)) = ((null), **in)}}
  // expected-warning@-12 {{pset(Pre(owner_const_ref)) = (*owner_const_ref)}}
  // expected-warning@-13 {{pset(Pre(*owner_const_ref)) = (**owner_const_ref)}}
  // expected-warning@-14 {{pset(Pre(int_ref)) = (*int_ref)}}
  // expected-warning@-15 {{pset(Pre(const_int_ref)) = (*const_int_ref)}}
  // expected-warning@-16 {{pset(Pre(ptr_ptr)) = ((null), *ptr_ptr)}}
  // expected-warning@-17 {{pset(Pre(*ptr_ptr)) = ((invalid))}}
  // expected-warning@-18 {{pset(Pre(ptr_by_value)) = ((null), *ptr_by_value)}}
  // expected-warning@-19 {{pset(Post(*ptr_ref)) = ((null), **owner_ref, **ptr_const_ref, *int_ref, *ptr_by_value)}}
  // expected-warning@-20 {{pset(Post(*ptr_ptr)) = ((null), **owner_ref, **ptr_const_ref, *int_ref, *ptr_by_value)}}
  __lifetime_contracts(p4);
  // expected-warning@-1 {{pset(Pre(a)) = ((null), *a)}}
  // expected-warning@-2 {{pset(Pre(b)) = ((null), *a)}}
  // expected-warning@-3 {{pset(Pre(c)) = (*c)}}
  // expected-warning@-4 {{pset(Pre(*c)) = ((invalid))}}
  // expected-warning@-5 {{pset(Post(*c)) = ((null), *a)}}
  __lifetime_contracts(p5);
  // expected-warning@-1 {{pset(Pre(a)) = ((null), *a)}}
  // expected-warning@-2 {{pset(Pre(b)) = ((null), *b)}}
  // expected-warning@-3 {{pset(Post((return value))) = ((null), *a, *b)}}
  __lifetime_contracts(p6);
  // expected-warning@-1 {{pset(Pre(a)) = ((null), *a)}}
  // expected-warning@-2 {{pset(Pre(b)) = ((null), *b)}}
  // expected-warning@-3 {{pset(Post((return value))) = ((null), *a)}}
  __lifetime_contracts(&S::f);
  // expected-warning@-1 {{pset(Pre(a)) = ((null), *a)}}
  // expected-warning@-2 {{pset(Pre(b)) = ((null), *b)}}
  // expected-warning@-3 {{pset(Pre(c)) = (*c)}}
  // expected-warning@-4 {{pset(Pre(*c)) = ((invalid))}}
  // expected-warning@-5 {{pset(Pre(this)) = (*this)}}
  // expected-warning@-6 {{pset(Post(*c)) = ((null), *a, *b)}}
  // expected-warning@-7 {{pset(Post((return value))) = ((null), *a, *b)}}
  __lifetime_contracts(&S::g);
  // expected-warning@-1 {{pset(Pre(a)) = ((null), *a)}}
  // expected-warning@-2 {{pset(Pre(b)) = ((null), *b)}}
  // expected-warning@-3 {{pset(Pre(c)) = (*c)}}
  // expected-warning@-4 {{pset(Pre(*c)) = ((invalid))}}
  // expected-warning@-5 {{pset(Pre(this)) = (*this)}}
  // expected-warning@-6 {{pset(Post(*c)) = ((null), *a, *b)}}
  // expected-warning@-7 {{pset(Post((return value))) = ((null), *this)}}
  __lifetime_contracts(p7);
  // expected-warning@-1 {{pset(Pre(a)) = ((null), *a)}}
  // expected-warning@-2 {{pset(Pre(b)) = ((null), *b)}}
  // expected-warning@-3 {{pset(Pre(c)) = (*c)}}
  // expected-warning@-4 {{pset(Pre(*c)) = ((invalid))}}
  // expected-warning@-5 {{pset(Post(*c)) = ((null), *a)}}
  __lifetime_contracts(p8);
  // expected-warning@-1 {{pset(Pre(a)) = ((null), *a)}}
  // expected-warning@-2 {{pset(Pre(b)) = ((null), *b)}}
  // expected-warning@-3 {{pset(Pre(c)) = ((null), *c)}}
  // expected-warning@-4 {{pset(Pre(*c)) = ((invalid))}}
  // expected-warning@-5 {{pset(Post(*c)) = ((null), *a)}}
  __lifetime_contracts(p9);
  // expected-warning@-1 {{pset(Pre(a)) = ((null), *a)}}
  // expected-warning@-2 {{pset(Pre(b)) = ((null), *b)}}
  // expected-warning@-3 {{pset(Pre(c)) = (*c)}}
  // expected-warning@-4 {{pset(Pre(*c)) = ((null), **c)}}
  __lifetime_contracts(p10);
  // expected-warning@-1 {{pset(Post((return value))) = ((null), (static))}}
}
} // namespace dump_contracts
