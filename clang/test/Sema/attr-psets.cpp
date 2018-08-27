// RUN: %clang_cc1 -fsyntax-only -Wlifetime -Wlifetime-debug -verify %s

// TODO:
// lifetime annotations
// lambda
// function calls

template <typename T>
bool __lifetime_pset(const T &);

template <typename T>
bool __lifetime_pset_ref(const T &);

namespace std {

template <typename T>
struct vector_iterator {
  T &operator*() const;
};

template <typename T>
struct vector {
  using iterator = vector_iterator<T>;
  vector(unsigned);
  iterator begin();
  iterator end();
  T &operator[](unsigned);
  T &at(unsigned);
  T *data();
  ~vector();
};

template <class T>
struct remove_reference { typedef T type; };
template <class T>
struct remove_reference<T &> { typedef T type; };
template <class T>
struct remove_reference<T &&> { typedef T type; };

template <typename T>
typename remove_reference<T>::type &&move(T &&arg);

template <typename T>
struct unique_ptr {
  T &operator*() const;
  explicit operator bool() const;
  ~unique_ptr();
};

template <typename T>
struct optional {
  T &value();
};

} // namespace std

namespace gsl {
template <typename T>
using nullable = T;

template <typename T>
struct not_null {
  constexpr operator T() const;
  constexpr T operator->() const;
};
} // namespace gsl

int rand();

struct S {
  ~S();
  int m;
  int *mp;
  static int s;
  void f() {
    int *p = &m;        // pset becomes m, not *this
    __lifetime_pset(p); // expected-warning {{pset(p) = (this.m)}}
    int *ps = &s;
    __lifetime_pset(ps); // expected-warning {{pset(ps) = ((static))}}
    int *ps2 = &this->s;
    __lifetime_pset(ps2); // expected-warning {{pset(ps2) = ((static))}}
    __lifetime_pset(mp);  // expected-warning {{pset(mp) = ((static))}}
  }
  int *get();
};

struct D : public S {
  ~D();
};

struct [[gsl::Pointer]] my_pointer {
  my_pointer();
  int operator*();
};

void pointer_exprs() {
  int *p;
  __lifetime_pset(p); // expected-warning {{pset(p) = ((invalid))}}
  int *q = nullptr;
  __lifetime_pset(q); // expected-warning {{pset(q) = ((null))}}
  int *q2 = 0;
  __lifetime_pset(q2); // expected-warning {{pset(q2) = ((null))}}

  int *p_zero{};           //zero initialization
  __lifetime_pset(p_zero); // expected-warning {{pset(p_zero) = ((null))}}

  S s;
  p = &s.m;
  __lifetime_pset(p); // expected-warning {{pset(p) = (s.m)}}
  int a[2];
  p = &a[0];
  __lifetime_pset(p); // expected-warning {{pset(p) = (a)}}

  D d;
  S *ps = &d;          // Ignore implicit cast
  __lifetime_pset(ps); // expected-warning {{pset(ps) = (d)}}

  D *pd = (D *)&s;     // Ignore explicit cast
  __lifetime_pset(pd); // expected-warning {{pset(pd) = (s)}}

  int i;
  p = q = &i;
  __lifetime_pset(p); // expected-warning {{pset(p) = (i)}}
  __lifetime_pset(q); // expected-warning {{pset(q) = (i)}}

  p = rand() % 2 ? &i : nullptr;
  __lifetime_pset(p); // expected-warning {{pset(p) = ((null), i)}}
}

void ref_exprs() {
  bool b;
  int i, j;
  int &ref1 = i;
  __lifetime_pset_ref(ref1); // expected-warning {{pset(ref1) = (i)}}

  int *p = &ref1;
  __lifetime_pset(p); // expected-warning {{pset(p) = (i)}}

  int &ref2 = b ? i : j;
  __lifetime_pset_ref(ref2); // expected-warning {{pset(ref2) = (i, j)}}

  // Lifetime extension
  const int &ref3 = 3;
  __lifetime_pset_ref(ref3); // expected-warning {{pset(ref3) = ((lifetime-extended temporary through ref3))}}

  // Lifetime extension of pointer
  int *const &refp = &i;
  __lifetime_pset_ref(refp); // expected-warning {{pset(refp) = ((lifetime-extended temporary through refp))}}
  p = refp;
  __lifetime_pset(p); // expected-warning {{pset(p) = (i)}}
}

void addr_and_dref() {
  int i;
  int *p = &i;
  __lifetime_pset(p); // expected-warning {{pset(p) = (i)}}

  int **p2 = &p;
  __lifetime_pset(p2); // expected-warning {{pset(p2) = (p)}}

  int ***p3 = &p2;
  __lifetime_pset(p3); // expected-warning {{pset(p3) = (p2)}}

  int **d2 = *p3;
  __lifetime_pset(d2); // expected-warning {{pset(d2) = (p)}}

  int *d1 = **p3;
  __lifetime_pset(d1); // expected-warning {{pset(d1) = (i)}}

  int **a = &**p3;
  __lifetime_pset(a); // expected-warning {{pset(a) = (p)}}

  int *b = **&*p3;
  __lifetime_pset(b); // expected-warning {{pset(b) = (i)}}

  int *c = ***&p3;
  __lifetime_pset(c); // expected-warning {{pset(c) = (i)}}
}

void forbidden() {

  int i;
  int *p = &i;
  p++;
  __lifetime_pset(p); // expected-warning {{pset(p) = ((invalid))}}

  p = &i;
  p--;
  __lifetime_pset(p); // expected-warning {{pset(p) = ((invalid))}}

  p = &i;
  ++p;
  __lifetime_pset(p); // expected-warning {{pset(p) = ((invalid))}}

  p = &i;
  --p;
  __lifetime_pset(p); // expected-warning {{pset(p) = ((invalid))}}

  p = &i + 3;
  __lifetime_pset(p); // expected-warning {{pset(p) = ((invalid))}}

  int *q = &p[3];
  __lifetime_pset(q); // expected-warning {{pset(q) = ((invalid))}}
}

void deref_array() {
  int *p[4];
  __lifetime_pset(p); // expected-warning {{pset(p) = ((invalid))}}
  int *k = p[1];
  __lifetime_pset(k); // expected-warning {{pset(k) = ((invalid))}}
}

int global_var;
void address_of_global() {
  int *p = &global_var;
  __lifetime_pset(p); // expected-warning {{pset(p) = ((static))}}
}

void class_type_pointer() {
  my_pointer p;       // default initialization
  __lifetime_pset(p); // expected-warning {{pset(p) = ((null))}}
}

void ref_leaves_scope() {
  int *p;
  {
    int i = 0;
    p = &i;
    __lifetime_pset(p); // expected-warning {{pset(p) = (i)}}
  }
  __lifetime_pset(p); // expected-warning {{pset(p) = ((invalid))}}
}

void pset_scope_if(bool bb) {
  int *p;
  int i, j;
  if (bb) {
    p = &i;
    __lifetime_pset(p); // expected-warning {{pset(p) = (i)}}
  } else {
    p = &j;
    __lifetime_pset(p); // expected-warning {{pset(p) = (j)}}
  }
  __lifetime_pset(p); // expected-warning {{pset(p) = (i, j)}}
}

void ref_leaves_scope_if(bool bb) {
  int *p;
  int j = 0;
  if (bb) {
    int i = 0;
    p = &i;
    __lifetime_pset(p); // expected-warning {{pset(p) = (i)}}
  } else {
    p = &j;
    __lifetime_pset(p); // expected-warning {{pset(p) = (j)}}
  }
  __lifetime_pset(p); // expected-warning {{pset(p) = ((invalid), j)}}
}

void ref_to_declarator_leaves_scope_if() {
  int *p;
  int j = 0;
  if (int i = 0) {
    p = &i;
    __lifetime_pset(p); // expected-warning {{pset(p) = (i)}}
  } else {
    p = &j;
    __lifetime_pset(p); // expected-warning {{pset(p) = (j)}}
  }
  __lifetime_pset(p); // expected-warning {{pset(p) = ((invalid))}}
}

void ignore_pointer_to_member() {
  int S::*mp = &S::m;       // pointer to member object; has no pset
  void (S::*mpf)() = &S::f; // pointer to member function; has no pset
}

void if_stmt(int *p, char *q, gsl::nullable<std::unique_ptr<int>> up) {
  __lifetime_pset(p);  // expected-warning {{pset(p) = ((null), p)}}
  __lifetime_pset(up); // expected-warning {{pset(up) = ((null), up')}}
  int *alwaysNull = nullptr;
  bool b = p && q;

  if (p) {
    __lifetime_pset(p); // expected-warning {{pset(p) = (p)}}
  } else {
    __lifetime_pset(p); // expected-warning {{pset(p) = ((null))}}
  }

  if (up) {
    __lifetime_pset(up); // expected-warning {{pset(up) = (up')}}
  } else {
    __lifetime_pset(up); // expected-warning {{pset(up) = ((null))}}
  }

  if (p != nullptr) {
    __lifetime_pset(p); // expected-warning {{pset(p) = (p)}}
  } else {
    __lifetime_pset(p); // expected-warning {{pset(p) = ((null))}}
  }

  if (p != alwaysNull) {
    __lifetime_pset(p); // expected-warning {{pset(p) = (p)}}
  } else {
    __lifetime_pset(p); // expected-warning {{pset(p) = ((null))}}
  }

  if (p && __lifetime_pset(p)) // expected-warning {{pset(p) = (p)}}
    ;

  if (!p || __lifetime_pset(p)) // expected-warning {{pset(p) = (p)}}
    ;

  p ? __lifetime_pset(p) : false;  // expected-warning {{pset(p) = (p)}}
  !p ? false : __lifetime_pset(p); // expected-warning {{pset(p) = (p)}}

  if (!p) {
    __lifetime_pset(p); // expected-warning {{pset(p) = ((null))}}
  } else {
    __lifetime_pset(p); // expected-warning {{pset(p) = (p)}}
  }

  if (p == nullptr) {
    __lifetime_pset(p); // expected-warning {{pset(p) = ((null))}}
  } else {
    __lifetime_pset(p); // expected-warning {{pset(p) = (p)}}
  }

  if (p && q) {
    __lifetime_pset(p); // expected-warning {{pset(p) = (p)}}
    __lifetime_pset(q); // expected-warning {{pset(q) = (q)}}
  } else {
    __lifetime_pset(p); // expected-warning {{pset(p) = ((null), p)}}
    __lifetime_pset(q); // expected-warning {{pset(q) = ((null), q)}}
  }

  if (!p || !q) {
    __lifetime_pset(p); // expected-warning {{pset(p) = ((null), p)}}
    __lifetime_pset(q); // expected-warning {{pset(q) = ((null), q)}}
  } else {
    __lifetime_pset(p); // expected-warning {{pset(p) = (p)}}
    __lifetime_pset(q); // expected-warning {{pset(q) = (q)}}
  }

  while (p) {
    __lifetime_pset(p); // expected-warning {{pset(p) = (p)}}
  }
}

void implicit_else() {
  int i = 0;
  int j = 0;
  int *p = rand() % 2 ? &j : nullptr;
  __lifetime_pset(p); // expected-warning {{pset(p) = ((null), j)}}

  if (!p) {
    p = &i;
  }

  __lifetime_pset(p); // expected-warning {{pset(p) = (i, j)}}
}

void condition_short_circuit(S *p) {
  if (p && p->m)
    ;
}

void switch_stmt() {
  int initial;
  int *p = &initial;
  __lifetime_pset(p); // expected-warning {{pset(p) = (initial)}}
  int i;
  int j;
  switch (i) {
  case 0:
    p = &i;
    __lifetime_pset(p); // expected-warning {{pset(p) = (i)}}
    break;
  case 1:
    int k;
    p = &k;
    __lifetime_pset(p); // expected-warning {{pset(p) = (k)}}
  case 2:
    p = &j;
    __lifetime_pset(p); // expected-warning {{pset(p) = (j)}}
  }
  __lifetime_pset(p); // expected-warning {{pset(p) = (initial, i, j)}}
}

// Duplicated warnings are due to the fact that we are doing fixed point
// iteration and some blocks might be visited multiple times.
void for_stmt() {
  int initial;
  int *p = &initial;
  __lifetime_pset(p); // expected-warning {{pset(p) = (initial)}}
  int j;
  // There are different psets on the first and further iterations.
  for (int i = 0; i < 1024; ++i) {
    __lifetime_pset(p); // expected-warning {{pset(p) = (initial)}}
                        // expected-warning@-1 {{pset(p) = (initial, j)}}
    p = &j;
  }
  __lifetime_pset(p); // expected-warning {{pset(p) = (initial)}}
                      // expected-warning@-1 {{pset(p) = (initial, j)}}
}

void for_stmt_ptr_decl() {
  int i;
  for (int *p = &i;;) {
    __lifetime_pset(p); // expected-warning {{pset(p) = (i)}}
  }
}

void goto_stmt(bool b) {
  int *p = nullptr;
  int i;
l1:
  __lifetime_pset(p); // expected-warning {{pset(p) = ((null))}}
                      // expected-warning@-1 {{pset(p) = ((null), i)}}
  p = &i;
  __lifetime_pset(p); // expected-warning {{pset(p) = (i)}}
                      // expected-warning@-1 {{pset(p) = (i)}}
  if (b)
    goto l1;

  __lifetime_pset(p); // expected-warning {{pset(p) = (i)}}
}

void goto_skipping_decl(bool b) {
  // When entering function start, there is no p;
  // when entering from the goto, there was a p
  int i;
l1:
  int *p = nullptr;
  __lifetime_pset(p); // expected-warning {{pset(p) = ((null))}}
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
e:;
  // We do not care about this case since the core guidelines forbid
  // the use of goto. TODO: do not crash on this.
  // __lifetime_pset(p); // TODOexpected-warning {{pset(p) = ((static))}}
}

void for_local_variable() {
  int i;
  int *p = &i;
  while (true) {
    __lifetime_pset(p); // expected-warning {{pset(p) = (i)}}
                        // expected-warning@-1 {{pset(p) = ((invalid), i)}}
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
  __lifetime_pset(p); // expected-warning {{pset(p) = ((null), p)}}
  assert(p);
  __lifetime_pset(p); // expected-warning {{pset(p) = (p)}}
}

int *global_p1 = nullptr;
int *global_p2 = nullptr;
int global_i;

void namespace_scoped_vars(int param_i, int *param_p) {
  __lifetime_pset(param_p);   // expected-warning {{pset(param_p) = ((null), param_p)}}
  __lifetime_pset(global_p1); // expected-warning {{pset(global_p1) = ((static))}}

  if (global_p1) {
    __lifetime_pset(global_p1); // expected-warning {{pset(global_p1) = ((static))}}
    *global_p1 = 3;
  }

  int local_i;
  global_p1 = &local_i; // expected-warning {{the pset of 'TODO' must be a subset of {(static), (null)}, but is {(local_i)}}
  global_p1 = &param_i; // expected-warning {{the pset of 'TODO' must be a subset of {(static), (null)}, but is {(param_i)}}
  global_p1 = param_p;  // expected-warning {{the pset of 'TODO' must be a subset of {(static), (null)}, but is {((null), param_p)}}
  int *local_p = global_p1;
  __lifetime_pset(local_p); // expected-warning {{pset(local_p) = ((static))}}

  global_p1 = nullptr;        // OK
  __lifetime_pset(global_p1); // expected-warning {{pset(global_p1) = ((static))}}
  global_p1 = &global_i;      // OK
  __lifetime_pset(global_p1); // expected-warning {{pset(global_p1) = ((static))}}
  global_p1 = global_p2;      // OK
  __lifetime_pset(global_p1); // expected-warning {{pset(global_p1) = ((static))}}
}

void function_call() {
  void f(int *p);

  int i;
  int *p = &i;
  __lifetime_pset(p); // expected-warning {{pset(p) = (i)}}
  f(p);
  __lifetime_pset(p); // expected-warning {{pset(p) = (i)}}
}

void function_call2() {
  void f(int **p);
  void const_f(int *const *p); //cannot change *p

  int i;
  int *p = &i;
  int **pp = &p;
  __lifetime_pset(p);  // expected-warning {{pset(p) = (i)}}
  __lifetime_pset(pp); // expected-warning {{pset(pp) = (p)}}

  const_f(pp);
  __lifetime_pset(pp); // expected-warning {{pset(pp) = (p)}}
  __lifetime_pset(p);  // expected-warning {{pset(p) = (i)}}

  f(pp);
  __lifetime_pset(pp); // expected-warning {{pset(pp) = (p)}}
  //__lifetime_pset(p);  // TODOexpected-warning {{pset(p) = (i, (static))}}
}

void function_call3() {
  void f(int *&p);

  int i;
  int *p = &i;
  __lifetime_pset(p); // expected-warning {{pset(p) = (i)}}
  f(p);
  //__lifetime_pset(p); // TOODexpected-warning {{pset(p) = (i, (static))}}
}

void indirect_function_call() {
  using F = int *(int *);
  F *f;
  int i = 0;
  int *p = &i;
  int *ret = f(p);
  __lifetime_pset(p); // expected-warning {{pset(p) = (i)}}
  //__lifetime_pset(ret); // TODOexpected-warning {{pset(p) = (i, (static))}}
}

void variadic_function_call() {
  void f(...);
  int i = 0;
  int *p = &i;
  f(p);
  __lifetime_pset(p); // expected-warning {{pset(p) = (i)}}
}

void member_function_call() {
  S s;
  // __lifetime_pset(s); // TODOexpected-warning {{pset(s) = (s')}}
  S *p = &s;
  __lifetime_pset(p); // expected-warning {{pset(p) = (s)}}
  int *pp = s.get();
  //__lifetime_pset(pp); // TODOexpected-warning {{pset(pp) = (s')}}
  s.f();              // non-const
  __lifetime_pset(p); // expected-warning {{pset(p) = (s)}}
  //__lifetime_pset(pp); // TODOexpected-warning {{pset(p) = ((invalid))}}
}

void argument_ref_to_temporary() {
  const int &min(const int &a, const int &b); //like std::min

  int x = 10, y = 2;
  const int &good = min(x, y); // ok, pset(good) == {x,y}
  //__lifetime_pset(good);   // TODOexpected-warning {{pset(good) = (x, y)}}

  const int &bad = min(x, y + 1);
  //__lifetime_pset(bad); // TODOexpected-warning {{pset(bad) = ((invalid))}}
}

void Example1_1() {
  int *p = nullptr;
  __lifetime_pset(p); // expected-warning {{pset(p) = ((null))}}
  int *p2;
  __lifetime_pset(p2); // expected-warning {{pset(p2) = ((invalid))}}
  {
    struct {
      int i;
    } s = {0};
    p = &s.i;
    __lifetime_pset(p); // expected-warning {{pset(p) = (s.i)}}
    p2 = p;
    __lifetime_pset(p2); // expected-warning {{pset(p2) = (s.i)}}
    *p = 1;              // ok
    *p2 = 1;             // ok
  }
  __lifetime_pset(p);  // expected-warning {{pset(p) = ((invalid))}}
  __lifetime_pset(p2); // expected-warning {{pset(p2) = ((invalid))}}
  p = nullptr;
  __lifetime_pset(p); // expected-warning {{pset(p) = ((null))}}
  int x[100];
  p2 = &x[10];
  __lifetime_pset(p2); // expected-warning {{pset(p2) = (x)}}
  *p2 = 1;             // ok
  p2 = p;              // D: pset(p2) = pset(p) which is {null}
  __lifetime_pset(p2); // expected-warning {{pset(p2) = ((null))}}
  p2 = &x[10];
  __lifetime_pset(p2); // expected-warning {{pset(p2) = (x)}}
  *p2 = 1;             // ok
  int **pp = &p2;
  __lifetime_pset(pp); // expected-warning {{pset(pp) = (p2)}}
  *pp = p;             // ok
}

void Example1_4() {
  int *p1 = nullptr;
  int *p2 = nullptr;
  int *p3 = nullptr;
  int *p4 = nullptr;
  {
    int i = 0;
    int &ri = i;
    __lifetime_pset_ref(ri); // expected-warning {{pset(ri) = (i)}}
    p1 = &ri;
    __lifetime_pset(p1); // expected-warning {{pset(p1) = (i)}}
    *p1 = 1;             // ok
    int *pi = &i;
    __lifetime_pset(pi); // expected-warning {{pset(pi) = (i)}}
    p2 = pi;
    __lifetime_pset(p2); // expected-warning {{pset(p2) = (i)}}
    *p2 = 1;             // ok
    int **ppi = &pi;
    __lifetime_pset(ppi); // expected-warning {{pset(ppi) = (pi)}}
    **ppi = 1;            // ok, modifies i
    int *pi2 = *ppi;
    __lifetime_pset(pi2); // expected-warning {{pset(pi2) = (i)}}
    p3 = pi2;
    __lifetime_pset(p3); // expected-warning {{pset(p3) = (i)}}
    *p3 = 1;             // ok
    {
      int j = 0;
      pi = &j;             // (note: so *ppi now points to j)
      __lifetime_pset(pi); // expected-warning {{pset(pi) = (j)}}
      pi2 = *ppi;
      __lifetime_pset(pi2); // expected-warning {{pset(pi2) = (j)}}
      **ppi = 1;            // ok, modifies j
      *pi2 = 1;             // ok
      p4 = pi2;
      __lifetime_pset(p4); // expected-warning {{pset(p4) = (j)}}
      *p4 = 1;             // ok
    }
    __lifetime_pset(pi);  // expected-warning {{pset(pi) = ((invalid))}}
    __lifetime_pset(pi2); // expected-warning {{pset(pi2) = ((invalid))}}
    __lifetime_pset(p4);  // expected-warning {{pset(p4) = ((invalid))}}
  }
  __lifetime_pset(p3); // expected-warning {{pset(p3) = ((invalid))}}
  __lifetime_pset(p2); // expected-warning {{pset(p2) = ((invalid))}}
  __lifetime_pset(p1); // expected-warning {{pset(p1) = ((invalid))}}
}

void Example9() {
  std::vector<int> v1(100);
  __lifetime_pset(v1); // expected-warning {{pset(v1) = (v1')}}
  int *pi = &v1[0];
  __lifetime_pset(pi); // expected-warning {{pset(pi) = (v1')}}
  pi = &v1.at(0);
  __lifetime_pset(pi); // expected-warning {{pset(pi) = (v1')}}
  auto v2 = std::move(v1);
  //__lifetime_pset(pi); // TODOexpected-warning {{pset(pi) = (v2')}}
}

void return_pointer() {
  std::vector<int> v1(100);
  __lifetime_pset(v1); // expected-warning {{pset(v1) = (v1')}}

  int *f(const std::vector<int> &);
  const std::vector<int> &id(const std::vector<int> &);
  void invalidate(std::vector<int> &);
  int *p = f(v1);
  __lifetime_pset(p); // expected-warning {{pset(p) = (v1')}}
  const std::vector<int> &v2 = id(v1);
  __lifetime_pset_ref(v2); // expected-warning {{pset(v2) = (v1)}}
  auto it = v1.begin();
  __lifetime_pset(it); // expected-warning {{pset(it) = (v1')}}

  int &r = v1[0];
  __lifetime_pset(r); // expected-warning {{pset(r) = (v1')}}
  __lifetime_pset(p); // expected-warning {{pset(p) = (v1')}}

  int *pmem = v1.data();
  __lifetime_pset(pmem); // expected-warning {{pset(pmem) = (v1')}}
  __lifetime_pset(p);    // expected-warning {{pset(p) = (v1')}}

  auto *v1p = &v1;
  __lifetime_pset(v1p); // expected-warning {{pset(v1p) = (v1)}}

  auto *pmem2 = v1p->data();
  __lifetime_pset(pmem2); // expected-warning {{pset(pmem2) = (v1')}}
  invalidate(v1);
  __lifetime_pset(p); // expected-warning {{pset(p) = ((invalid))}}

  int *g(int *, float *, float **);
  int a;
  float b;
  float *c;
  int *q = g(&a, &b, &c);
  __lifetime_pset(q); // expected-warning {{pset(q) = (a)}}
  __lifetime_pset(c); // expected-warning {{pset(c) = (b)}}
}

void test_annotations(gsl::nullable<int *> p, gsl::not_null<int *> q) {
  __lifetime_pset(p); // expected-warning {{pset(p) = ((null), p)}}
  __lifetime_pset(q); // expected-warning {{pset(q) = (q)}}
}

void ambiguous_pointers(bool cond) {
  int x;
  int y;
  int z;
  int w;
  int *p1 = &x;
  int *p2 = &y;
  int **pp = &p1;
  __lifetime_pset(p1); // expected-warning {{pset(p1) = (x)}}
  __lifetime_pset(p2); // expected-warning {{pset(p2) = (y)}}
  __lifetime_pset(pp); // expected-warning {{pset(pp) = (p1)}}
  *pp = &w;
  __lifetime_pset(p1); // expected-warning {{pset(p1) = (w)}}
  __lifetime_pset(p2); // expected-warning {{pset(p2) = (y)}}
  __lifetime_pset(pp); // expected-warning {{pset(pp) = (p1)}}
  if (cond)
    pp = &p2;
  *pp = &z;
  __lifetime_pset(p1); // expected-warning {{pset(p1) = (z, w)}}
  __lifetime_pset(p2); // expected-warning {{pset(p2) = (y, z)}}
  __lifetime_pset(pp); // expected-warning {{pset(pp) = (p1, p2)}}
}

void cast(int *p) {
  float *q = reinterpret_cast<float *>(p);
  __lifetime_pset(q); // expected-warning {{pset(q) = ((invalid))}}
}

// Support CXXOperatorCallExpr on non-member function
struct S3;
void operator==(S3 &A, S3 &B) { A == B; }

struct S2 {
  void f() {
    this->f();
    (*this).f();
  }
};

void self_init() {
  int *a = a;
}

class A {
  int *B = nullptr;
  int *C;
  A() : C() {}
};

void deref_based_on_template_param() {
  // we determine DerefType(std::optional) by looking at it's template parameter
  std::optional<int> O;
  __lifetime_pset(O); // expected-warning {{pset(O) = (O')}}
  int &D = O.value();
  __lifetime_pset(D); // expected-warning {{pset(D) = (O')}}
  D = 1;

  int &f_ref(const std::optional<int> &O);
  int &D2 = f_ref(O);
  __lifetime_pset(D2); // expected-warning {{pset(D2) = (O')}}

  int *f_ptr(const std::optional<int> &O);
  int *D3 = f_ptr(O);
  __lifetime_pset(D3); //expected-warning {{pset(D3) = (O')}}
}