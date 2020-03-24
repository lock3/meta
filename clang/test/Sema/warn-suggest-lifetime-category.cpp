// RUN: %clang_cc1 -fsyntax-only -Wsuggest-lifetime-category -verify %s

// Owner: Has begin(), end() and a non-trivial destructor.
struct O1 { // expected-warning {{could be annotate as [[gsl::Owner]]}}
  int *begin();
  int *end();
  ~O1();
};

// Owner: Has an operator-> and a user-provided destructor.
struct O2 { // expected-warning {{could be annotate as [[gsl::Owner]]}}
  int *operator->();
  ~O2();
};

// Owner: Has an unary operator* and a user-provided destructor.
struct O3 { // expected-warning {{could be annotate as [[gsl::Owner]]}}
  int *operator*();
  ~O3();
};

// Owner: Derived from an Owner.
struct [[gsl::Owner]] OwnerBase{};
struct O4 : public OwnerBase {}; // expected-warning {{could be annotate as [[gsl::Owner]]}}

// Pointer: Has begin(), end() and a trivial destructor.
struct P1 { // expected-warning {{could be annotate as [[gsl::Pointer]]}}
  int *begin();
  int *end();
};

// Pointer: Has an unary operator* and operator++.
struct P2 { // expected-warning {{could be annotate as [[gsl::Pointer]]}}
  int *operator*();
  P2 &operator++();
};

// Pointer: Has an operator-> and a trivial destructor.
struct P3 { // expected-warning {{could be annotate as [[gsl::Pointer]]}}
  int *operator->();
};

// Pointer: Has an unary operator* and a trivial destructor.
struct P4 { // expected-warning {{could be annotate as [[gsl::Pointer]]}}
  int *operator*();
};

// Pointer: Derived from an Pointer.
struct [[gsl::Pointer]] PointerBase{};
struct P5 : public PointerBase {}; // expected-warning {{could be annotate as [[gsl::Pointer]]}}

// Template instantiation
template <typename T>
class P6 { // expected-warning {{could be annotate as [[gsl::Pointer]]}}
  T operator*();
};

P6<int *> p6; // expected-note {{in instantiation}}
