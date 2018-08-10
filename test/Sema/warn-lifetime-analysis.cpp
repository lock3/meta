// RUN: %clang_cc1 -fsyntax-only -verify -Wlifetime %s

struct S {
  ~S();
  int m;
  int f();
};

struct [[gsl::Pointer]] my_pointer {
  int operator*();
};

void deref_uninitialized() {
  int *p; // expected-note {{it was never initialized here}}
  *p = 3; // expected-warning {{dereferencing a dangling pointer}}
  my_pointer p2;
  *p2;
}

void deref_nullptr() {
  int *q = nullptr;
  *q = 3; // expected-warning {{dereferencing a null pointer}}
}

void ref_leaves_scope() {
  int *p;
  {
    int i = 0;
    p = &i;
    *p = 2; // OK
  }         // expected-note {{pointee 'i' left the scope here}}
  *p = 1;   // expected-warning {{dereferencing a dangling pointer}}
}

void ref_to_member_leaves_scope_call() {
  S *p;
  {
    S s;
    p = &s;
    p->f();     // OK
  }             // expected-note 3 {{pointee 's' left the scope here}}
  p->f();       // expected-warning {{dereferencing a dangling pointer}}
  int i = p->m; // expected-warning {{dereferencing a dangling pointer}}
  p->m = 4;     // expected-warning {{dereferencing a dangling pointer}}
}

// No Pointer involved, thus not checked.
void ignore_access_on_non_ref_ptr() {
  S s;
  s.m = 3;
  s.f();
}

// Note: the messages below are for the template instantiation in 'instantiate_ref_leaves_scope_template'.
// The checker only checks instantiations.
// TODO: some parts of the templated functions that are not dependent on the
//       template argument could be checked independently of the
//       instantiations.
template <typename T>
void ref_leaves_scope_template() {
  T p;
  {
    int i = 0;
    p = &i;
    *p = 2; // OK
  }         // expected-note {{pointee 'i' left the scope here}}
  *p = 1;   // expected-warning {{dereferencing a dangling pointer}}
}

void instantiate_ref_leaves_scope_template() {
  ref_leaves_scope_template<int *>(); // expected-note {{in instantiation of}}
}

int global_i = 4;
int *global_init_p = &global_i; // OK
int *global_uninit_p;           // TODOexpected-warning {{the pset of 'global_uninit_p' must be a subset of {(static), (null)}, but is {(invalid)}}
int *global_null_p = nullptr;   // OK

void uninitialized_static() {
  static int *p; // OK, statics initialize to null
}

void function_call() {
  void f(int *);
  void g(int **);
  void h(int *, int **);

  int *p; // expected-note {{it was never initialized here}}
  f(p);   // expected-warning {{passing a dangling pointer as argument}}

  int **q = &p;
  //TODO: should this diagnose? If I understood Herb correctly,
  // then q must be valid, but *q is an out parameter, and thus is allowed be invalid (will be written, not read, in callee).
  g(q); // TODOexpected-warning {{passing a indirectly dangling pointer as parameter}} // TODOexpected-note {{was dereferenced here}}

  int i;
  p = &i;
  h(p, q); // expected-warning {{this argument points to the same variable 'i' as another argument}} expected-note {{here}}
}