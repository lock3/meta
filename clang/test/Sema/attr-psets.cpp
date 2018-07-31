// RUN: %clang_cc1 -fsyntax-only -Wlifetime -Wlifetime-debug -verify %s

// TODO:
// lifetime annotations
// lambda
// function calls

template <typename T>
void clang_analyzer_pset(const T &);

int rand();

struct S {
  ~S();
  int m;
  void f() {
    int *p = &m;            // pset becomes m, not *this
    clang_analyzer_pset(p); // expected-warning {{pset(p) = m}}
  }
};

struct D : public S {
  ~D();
};

void pointer_exprs() {
  int *p;
  clang_analyzer_pset(p); // expected-warning {{pset(p) = (invalid)}}
  int *q = nullptr;
  clang_analyzer_pset(q); // expected-warning {{pset(q) = (null)}}
  int *q2 = 0;
  clang_analyzer_pset(q2); // expected-warning {{pset(q2) = (null)}}
  S s;
  p = &s.m;
  clang_analyzer_pset(p); // expected-warning {{pset(p) = s}}
  int a[2];
  p = &a[0];
  clang_analyzer_pset(p); // expected-warning {{pset(p) = a}}

  D d;
  S *ps = &d;              // Ignore implicit cast
  clang_analyzer_pset(ps); // expected-warning {{pset(ps) = d}}

  D *pd = (D *)&s;         // Ignore explicit cast
  clang_analyzer_pset(pd); // expected-warning {{pset(pd) = s}}

  int i;
  p = q = &i;
  clang_analyzer_pset(p); // expected-warning {{pset(p) = i}}
  clang_analyzer_pset(q); // expected-warning {{pset(q) = i}}

  p = rand() % 2 ? &i : nullptr;
  clang_analyzer_pset(p); // expected-warning {{pset(p) = (null), i}}
}

void ref_exprs() {
  bool b;
  int i, j;
  int &ref1 = i;
  clang_analyzer_pset(ref1); // expected-warning {{pset(ref1) = i}}

  int *p = &ref1;
  clang_analyzer_pset(p); // expected-warning {{pset(p) = i}}

  int &ref2 = b ? i : j;
  clang_analyzer_pset(ref2); // expected-warning {{pset(ref2) = i, j}}

  // Lifetime extension
  const int &ref3 = 3;
  clang_analyzer_pset(ref3); // expected-warning {{pset(ref3) = ref3'}}

  // Lifetime extension of pointer; TODO is that correct?
  int *const &refp = &i;
  clang_analyzer_pset(refp); // expected-warning {{pset(refp) = refp'}}
}

void addr_and_dref() {
  int i;
  int *p = &i;
  clang_analyzer_pset(p); // expected-warning {{pset(p) = i}}

  int **p2 = &p;
  clang_analyzer_pset(p2); // expected-warning {{pset(p2) = p}}

  int ***p3 = &p2;
  clang_analyzer_pset(p3); // expected-warning {{pset(p3) = p2}}

  int **d2 = *p3;
  clang_analyzer_pset(d2); // expected-warning {{pset(d2) = p}}

  int *d1 = **p3;
  clang_analyzer_pset(d1); // expected-warning {{pset(d1) = i}}

  int **a = &**p3;
  clang_analyzer_pset(a); // expected-warning {{pset(a) = p}}

  int *b = **&*p3;
  clang_analyzer_pset(b); // expected-warning {{pset(b) = i}}

  int *c = ***&p3;
  clang_analyzer_pset(c); // expected-warning {{pset(c) = i}}
}

void forbidden() {

  int i;
  int *p = &i;
  p++;
  clang_analyzer_pset(p); // expected-warning {{pset(p) = (unknown)}}

  p = &i;
  p--;
  clang_analyzer_pset(p); // expected-warning {{pset(p) = (unknown)}}

  p = &i;
  ++p;
  clang_analyzer_pset(p); // expected-warning {{pset(p) = (unknown)}}

  p = &i;
  --p;
  clang_analyzer_pset(p); // expected-warning {{pset(p) = (unknown)}}

  p = &i + 3;
  clang_analyzer_pset(p); // expected-warning {{pset(p) = (unknown)}}

  int *q = &p[3];
  clang_analyzer_pset(q); // expected-warning {{pset(q) = (unknown)}}
}

void ref_leaves_scope() {
  int *p;
  {
    int i = 0;
    p = &i;
    clang_analyzer_pset(p); // expected-warning {{pset(p) = i}}
  }
  clang_analyzer_pset(p); // expected-warning {{pset(p) = (invalid)}}
}

void pset_scope_if(bool bb) {
  int *p;
  int i, j;
  if (bb) {
    p = &i;
    clang_analyzer_pset(p); // expected-warning {{pset(p) = i}}
  } else {
    p = &j;
    clang_analyzer_pset(p); // expected-warning {{pset(p) = j}}
  }
  clang_analyzer_pset(p); // expected-warning {{pset(p) = i, j}}
}

void ref_leaves_scope_if(bool bb) {
  int *p;
  int j = 0;
  if (bb) {
    int i = 0;
    p = &i;
    clang_analyzer_pset(p); // expected-warning {{pset(p) = i}}
  } else {
    p = &j;
    clang_analyzer_pset(p); // expected-warning {{pset(p) = j}}
  }
  clang_analyzer_pset(p); // expected-warning {{pset(p) = (invalid)}}
}

void ref_to_declarator_leaves_scope_if() {
  int *p;
  int j = 0;
  if (int i = 0) {
    p = &i;
    clang_analyzer_pset(p); // expected-warning {{pset(p) = i}}
  } else {
    p = &j;
    clang_analyzer_pset(p); // expected-warning {{pset(p) = j}}
  }
  clang_analyzer_pset(p); // expected-warning {{pset(p) = (invalid)}}
}

void ignore_pointer_to_member() {
  int S::*mp = &S::m;       // pointer to member object; has no pset
  void (S::*mpf)() = &S::f; // pointer to member function; has no pset
}

void if_stmt(int *p) {
  clang_analyzer_pset(p); // expected-warning {{pset(p) = (null), p'}}

  if (p) {
    clang_analyzer_pset(p); // expected-warning {{pset(p) = p'}}
  } else {
    clang_analyzer_pset(p); // expected-warning {{pset(p) = (null), p'}}
  }

  if (!p) {
    clang_analyzer_pset(p); // expected-warning {{pset(p) = (null), p'}}
  } else {
    clang_analyzer_pset(p); // expected-warning {{pset(p) = p'}}
  }

  char *q;
  if (p && q) {
    clang_analyzer_pset(p); // expected-warning {{pset(p) = p'}}
  } else {
    clang_analyzer_pset(p); // expected-warning {{pset(p) = (null), p'}}
  }

  if (!p || !q) {
    clang_analyzer_pset(p); // expected-warning {{pset(p) = (null), p'}}
  } else {
    clang_analyzer_pset(p); // expected-warning {{pset(p) = p'}}
  }
}

void implicit_else() {
  int i = 0;
  int j = 0;
  int *p = rand() % 2 ? &j : nullptr;
  clang_analyzer_pset(p); // expected-warning {{pset(p) = (null), j}}

  if (!p) {
    p = &i;
  }

  clang_analyzer_pset(p); // expected-warning {{pset(p) = i, j}}
}

void condition_short_circuit(S *p) {
  // TODO: should not warn
  if (p && p->m)
    ;
}

void switch_stmt() {
  int initial;
  int *p = &initial;
  clang_analyzer_pset(p); // expected-warning {{pset(p) = initial}}
  int i;
  int j;
  switch (i) {
  case 0:
    p = &i;
    clang_analyzer_pset(p); // expected-warning {{pset(p) = i}}
    break;
  case 1:
    int k;
    p = &k;
    clang_analyzer_pset(p); // expected-warning {{pset(p) = k}}
  case 2:
    p = &j;
    clang_analyzer_pset(p); // expected-warning {{pset(p) = j}}
  }
  clang_analyzer_pset(p); // expected-warning {{pset(p) = initial, i, j}}
}

void for_stmt() {
  int initial;
  int *p = &initial;
  clang_analyzer_pset(p); // expected-warning {{pset(p) = initial}}
  int j;
  // There are different psets on the first and further iterations.
  for (int i = 0; i < 1024; ++i) {
    clang_analyzer_pset(p); // expected-warning {{pset(p) = initial, j}}
    p = &j;
  }
  clang_analyzer_pset(p); // expected-warning {{pset(p) = initial, j}}
}

void for_stmt_ptr_decl() {
  int i;
  for (int *p = &i;;) {
    clang_analyzer_pset(p); // expected-warning {{pset(p) = i}}
  }
}

void goto_stmt(bool b) {
  int *p = nullptr;
  int i;
l1:
  clang_analyzer_pset(p); // expected-warning {{pset(p) = (null), i}}
  p = &i;
  clang_analyzer_pset(p); // expected-warning {{pset(p) = i}}
  if (b)
    goto l1;

  clang_analyzer_pset(p); // expected-warning {{pset(p) = i}}
}

void goto_skipping_decl(bool b) {
  // When entering function start, there is no p;
  // when entering from the goto, there was a p
  int i;
l1:
  int *p = nullptr;
  clang_analyzer_pset(p); // expected-warning {{pset(p) = (null)}}
  if (b)
    p = &i;
  goto l1;
}

void goto_forward_over_decl() {
  // TODO: When jumping over the declaration of p, we will never see that
  // DeclStmt in the CFG
  int j;
  goto e;
  int *p;
e:
  clang_analyzer_pset(p); // expected-warning {{pset(p) = (invalid)}} // TODO
}

void for_local_variable() {
  int i;
  int *p = &i;
  while (true) {
    clang_analyzer_pset(p); // expected-warning {{pset(p) = (invalid)}}
    int j;
    p = &j; // p will become invalid on the next iteration, because j leaves scope
  }
}

//Simplified from assert.h
void __assert_fail() __attribute__((__noreturn__));
#define assert(expr)          \
  ((expr)                     \
       ? static_cast<void>(0) \
       : __assert_fail())

void asserting(int *p) {
  clang_analyzer_pset(p); // expected-warning {{pset(p) = (null), p'}}
  assert(p);
  clang_analyzer_pset(p); // expected-warning {{pset(p) = p'}}
}

int *global_p1 = nullptr;
int *global_p2 = nullptr;
int global_i;

void namespace_scoped_vars(int param_i, int *param_p) {
  clang_analyzer_pset(param_p); // expected-warning {{pset(param_p) = (null), param_p'}}

  if (global_p1) {
    clang_analyzer_pset(global_p1); // expected-warning {{pset(global_p1) = (static)}}
    *global_p1 = 3;
  }

  int local_i;
  global_p1 = &local_i; // expected-warning {{the pset of 'global_p1' must be a subset of {(static), (null)}, but is {local_i}}
  global_p1 = &param_i; // expected-warning {{the pset of 'global_p1' must be a subset of {(static), (null)}, but is {param_i}}
  global_p1 = param_p;  // expected-warning {{the pset of 'global_p1' must be a subset of {(static), (null)}, but is {(null), param_p'}}

  global_p1 = nullptr;            // OK
  clang_analyzer_pset(global_p1); // expected-warning {{pset(global_p1) = (null)}}
  global_p1 = &global_i;          // OK
  clang_analyzer_pset(global_p1); // expected-warning {{pset(global_p1) = (static)}}
  global_p1 = global_p2;          // OK
  clang_analyzer_pset(global_p1); // expected-warning {{pset(global_p1) = (null), (static)}}
}

void function_call() {
  void f(int *p);

  int i;
  int *p = &i;
  clang_analyzer_pset(p); // expected-warning {{pset(p) = i}}
  f(p);
  clang_analyzer_pset(p); // expected-warning {{pset(p) = i}}
}

void function_call2() {
  void f(int **p);
  void const_f(int *const *p); //cannot change *p

  int i;
  int *p = &i;
  int **pp = &p;
  clang_analyzer_pset(p);  // expected-warning {{pset(p) = i}}
  clang_analyzer_pset(pp); // expected-warning {{pset(pp) = p}}

  const_f(pp);
  clang_analyzer_pset(pp); // expected-warning {{pset(pp) = p}}
  clang_analyzer_pset(p);  // expected-warning {{pset(p) = i}}

  f(pp);
  clang_analyzer_pset(pp); // expected-warning {{pset(pp) = p}}
  clang_analyzer_pset(p);  // expected-warning {{pset(p) = i, static)}} //TODO
}

void function_call3() {
  void f(int *&p);

  int i;
  int *p = &i;
  clang_analyzer_pset(p); // expected-warning {{pset(p) = i}}
  f(p);
  clang_analyzer_pset(p); // expected-warning {{pset(p) = i, static)}} //TODO
}

void indirect_function_call() {
  using F = int*(int*);
  F* f;
  int i = 0;
  int* p = &i;
  int* ret = f(p);
  clang_analyzer_pset(p); // expected-warning {{pset(p) = i}}
  clang_analyzer_pset(ret); // expected-warning {{pset(p) = i, static)}} //TODO
}

void variadic_function_call() {
  void f(...);
  int i = 0;
  int* p = &i;
  f(p);
  clang_analyzer_pset(p); // expected-warning {{pset(p) = i}}
}

void member_function_call() {
  S s;
  S* p = &s;
  clang_analyzer_pset(p); // expected-warning {{pset(p) = s}}
  s.f(); // non-const
  clang_analyzer_pset(p); // expected-warning {{pset(p) = (invalid)}}
}

void argument_ref_to_temporary() {
  const int &min(const int &a, const int &b); //like std::min

  int x = 10, y = 2;
  const int &good = min(x, y); // ok, pset(good) == {x,y}
  clang_analyzer_pset(good);   // expected-warning {{pset(good) = x, y}} // TODO

  const int &bad = min(x, y + 1);
  clang_analyzer_pset(bad); // expected-warning {{pset(bad) = (invalid)}} // TODO
}
