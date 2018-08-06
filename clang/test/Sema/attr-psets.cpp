// RUN: %clang_cc1 -fsyntax-only -Wlifetime -Wlifetime-debug -verify %s

// TODO:
// lifetime annotations
// lambda
// function calls

template <typename T>
bool clang_analyzer_pset(const T &);

namespace std {
template <typename T>
struct vector {
  vector(unsigned);
  T *begin();
  T *end();
  T &operator[](unsigned);
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
} // namespace std

int rand();

struct S {
  ~S();
  int m;
  static int s;
  void f() {
    int *p = &m;            // pset becomes m, not *this
    clang_analyzer_pset(p); // expected-warning {{pset(p) = this.m}}
    int *ps = &s;
    clang_analyzer_pset(ps); // expected-warning {{pset(ps) = (static)}}
    int *ps2 = &this->s;
    clang_analyzer_pset(ps2); // TODO expected-warning {{pset(ps2) = (static)}}
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
  clang_analyzer_pset(p); // expected-warning {{pset(p) = (invalid)}}
  int *q = nullptr;
  clang_analyzer_pset(q); // expected-warning {{pset(q) = (null)}}
  int *q2 = 0;
  clang_analyzer_pset(q2); // expected-warning {{pset(q2) = (null)}}

  int *p_zero{};               //zero initialization
  clang_analyzer_pset(p_zero); // expected-warning {{pset(p_zero) = (null)}}

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
  clang_analyzer_pset(ref3); // expected-warning {{pset(ref3) = (lifetime-extended temporary through ref3)}}

  // Lifetime extension of pointer; TODO is that correct?
  int *const &refp = &i;
  clang_analyzer_pset(refp); // expected-warning {{pset(refp) = (lifetime-extended temporary through refp)}}
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
  clang_analyzer_pset(p); // expected-warning {{pset(p) = (invalid)}}

  p = &i;
  p--;
  clang_analyzer_pset(p); // expected-warning {{pset(p) = (invalid)}}

  p = &i;
  ++p;
  clang_analyzer_pset(p); // expected-warning {{pset(p) = (invalid)}}

  p = &i;
  --p;
  clang_analyzer_pset(p); // expected-warning {{pset(p) = (invalid)}}

  p = &i + 3;
  //clang_analyzer_pset(p); // TODOexpected-warning {{pset(p) = (invalid)}}

  int *q = &p[3];
  //clang_analyzer_pset(q); // TODOexpected-warning {{pset(q) = (invalid)}}
}

void deref_array() {
  int *p[4];
  clang_analyzer_pset(p); // expected-warning {{pset(p) = (untracked)}}
  int *k = *p;            // expected-note {{pointer arithmetic is not allowed}} expected-warning {{dereferencing a dangling pointer}}
  clang_analyzer_pset(k); // expected-warning {{pset(k) = (invalid)}}
}

int global_var;
void address_of_global() {
  int *p = &global_var;
  clang_analyzer_pset(p); // expected-warning {{pset(p) = (static)}}
}

void class_type_pointer() {
  my_pointer p;           // default initialization
  clang_analyzer_pset(p); // expected-warning {{pset(p) = (null)}}
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

void if_stmt(int *p, char *q) {
  clang_analyzer_pset(p); // expected-warning {{pset(p) = (null), p}}
  int *alwaysNull = nullptr;

  if (p) {
    clang_analyzer_pset(p); // expected-warning {{pset(p) = p}}
  } else {
    clang_analyzer_pset(p); // expected-warning {{pset(p) = (null)}}
  }

  if (p != nullptr) {
    clang_analyzer_pset(p); // expected-warning {{pset(p) = p}}
  } else {
    clang_analyzer_pset(p); // expected-warning {{pset(p) = (null)}}
  }

  if (p != alwaysNull) {
    clang_analyzer_pset(p); // expected-warning {{pset(p) = p}}
  } else {
    clang_analyzer_pset(p); // expected-warning {{pset(p) = (null)}}
  }

  if (p && clang_analyzer_pset(p)) // expected-warning {{pset(p) = p}}
    ;

  if (!p || clang_analyzer_pset(p)) // expected-warning {{pset(p) = p}}
    ;

  p ? clang_analyzer_pset(p) : false;  // expected-warning {{pset(p) = p}}
  !p ? false : clang_analyzer_pset(p); // expected-warning {{pset(p) = p}}

  if (!p) {
    clang_analyzer_pset(p); // expected-warning {{pset(p) = (null)}}
  } else {
    clang_analyzer_pset(p); // expected-warning {{pset(p) = p}}
  }

  if (p == nullptr) {
    clang_analyzer_pset(p); // expected-warning {{pset(p) = (null)}}
  } else {
    clang_analyzer_pset(p); // expected-warning {{pset(p) = p}}
  }

  if (p && q) {
    clang_analyzer_pset(p); // expected-warning {{pset(p) = p}}
    clang_analyzer_pset(q); // expected-warning {{pset(q) = q}}
  } else {
    clang_analyzer_pset(p); // expected-warning {{pset(p) = (null), p}}
    clang_analyzer_pset(q); // expected-warning {{pset(q) = (null), q}}
  }

  if (!p || !q) {
    clang_analyzer_pset(p); // expected-warning {{pset(p) = (null), p}}
    clang_analyzer_pset(q); // expected-warning {{pset(q) = (null), q}}
  } else {
    clang_analyzer_pset(p); // expected-warning {{pset(p) = p}}
    clang_analyzer_pset(q); // expected-warning {{pset(q) = q}}
  }

  while (p) {
    clang_analyzer_pset(p); // expected-warning {{pset(p) = p}}
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
e:;
  //clang_analyzer_pset(p); // TODOexpected-warning {{pset(p) = (invalid)}}
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
  clang_analyzer_pset(p); // expected-warning {{pset(p) = (null), p}}
  assert(p);
  clang_analyzer_pset(p); // expected-warning {{pset(p) = p}}
}

int *global_p1 = nullptr;
int *global_p2 = nullptr;
int global_i;

void namespace_scoped_vars(int param_i, int *param_p) {
  clang_analyzer_pset(param_p); // expected-warning {{pset(param_p) = (null), param_p}}

  if (global_p1) {
    clang_analyzer_pset(global_p1); // expected-warning {{pset(global_p1) = (static)}}
    *global_p1 = 3;
  }

  int local_i;
  global_p1 = &local_i; // expected-warning {{the pset of 'global_p1' must be a subset of {(static), (null)}, but is {local_i}}
  global_p1 = &param_i; // expected-warning {{the pset of 'global_p1' must be a subset of {(static), (null)}, but is {param_i}}
  global_p1 = param_p;  // expected-warning {{the pset of 'global_p1' must be a subset of {(static), (null)}, but is {(null), param_p}}

  //TODO global_p1 = nullptr;            // OK
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
  //clang_analyzer_pset(p);  // TODOexpected-warning {{pset(p) = i, static)}}
}

void function_call3() {
  void f(int *&p);

  int i;
  int *p = &i;
  clang_analyzer_pset(p); // expected-warning {{pset(p) = i}}
  f(p);
  //clang_analyzer_pset(p); // TOODexpected-warning {{pset(p) = i, static)}}
}

void indirect_function_call() {
  using F = int *(int *);
  F *f;
  int i = 0;
  int *p = &i;
  int *ret = f(p);
  clang_analyzer_pset(p); // expected-warning {{pset(p) = i}}
  //clang_analyzer_pset(ret); // TODOexpected-warning {{pset(p) = i, static)}}
}

void variadic_function_call() {
  void f(...);
  int i = 0;
  int *p = &i;
  f(p);
  clang_analyzer_pset(p); // expected-warning {{pset(p) = i}}
}

void member_function_call() {
  S s;
  // clang_analyzer_pset(s); // TODOexpected-warning {{pset(s) = s'}}
  S *p = &s;
  clang_analyzer_pset(p); // expected-warning {{pset(p) = s}}
  int *pp = s.get();
  //clang_analyzer_pset(pp); // TODOexpected-warning {{pset(pp) = s'}}
  s.f();                  // non-const
  clang_analyzer_pset(p); // expected-warning {{pset(p) = s}}
  //clang_analyzer_pset(pp); // TODOexpected-warning {{pset(p) = (invalid)}}
}

void argument_ref_to_temporary() {
  const int &min(const int &a, const int &b); //like std::min

  int x = 10, y = 2;
  const int &good = min(x, y); // ok, pset(good) == {x,y}
  //clang_analyzer_pset(good);   // TODOexpected-warning {{pset(good) = x, y}}

  const int &bad = min(x, y + 1);
  //clang_analyzer_pset(bad); // TODOexpected-warning {{pset(bad) = (invalid)}}
}

void Example1_1() {
  int *p = nullptr;
  clang_analyzer_pset(p); // expected-warning {{pset(p) = (null)}}
  int *p2;
  clang_analyzer_pset(p2); // expected-warning {{pset(p2) = (invalid)}}
  {
    struct {
      int i;
    } s = {0};
    p = &s.i;
    //clang_analyzer_pset(p); // TODOexpected-warning {{pset(p) = s.i}}
    p2 = p;
    //clang_analyzer_pset(p2); // TODOexpected-warning {{pset(p2) = s.i}}
    *p = 1;                  // ok
    *p2 = 1;                 // ok
  }
  clang_analyzer_pset(p);  // expected-warning {{pset(p) = (invalid)}}
  clang_analyzer_pset(p2); // expected-warning {{pset(p2) = (invalid)}}
  p = nullptr;
  clang_analyzer_pset(p); // expected-warning {{pset(p) = (null)}}
  int x[100];
  p2 = &x[10];
  clang_analyzer_pset(p2); // expected-warning {{pset(p2) = x}}
  *p2 = 1;                 // ok
  p2 = p;                  // D: pset(p2) = pset(p) which is {null}
  clang_analyzer_pset(p2); // expected-warning {{pset(p2) = (null)}}
  p2 = &x[10];
  clang_analyzer_pset(p2); // expected-warning {{pset(p2) = x}}
  *p2 = 1;                 // ok
  int **pp = &p2;
  clang_analyzer_pset(pp); // expected-warning {{pset(pp) = p}}
  *pp = p;                 // ok
}

void Example1_4() {
  int *p1 = nullptr;
  int *p2 = nullptr;
  int *p3 = nullptr;
  int *p4 = nullptr;
  {
    int i = 0;
    int &ri = i;
    clang_analyzer_pset(ri); // expected-warning {{pset(ri) = i}}
    p1 = &ri;
    clang_analyzer_pset(p1); // expected-warning {{pset(p1) = i}}
    *p1 = 1;                 // ok
    int *pi = &i;
    clang_analyzer_pset(pi); // expected-warning {{pset(pi) = i}}
    p2 = pi;
    clang_analyzer_pset(p2); // expected-warning {{pset(p2) = i}}
    *p2 = 1;                 // ok
    int **ppi = &pi;
    clang_analyzer_pset(ppi); // expected-warning {{pset(ppi) = pi}}
    **ppi = 1;                // ok, modifies i
    int *pi2 = *ppi;
    clang_analyzer_pset(pi2); // expected-warning {{pset(pi2) = i}}
    p3 = pi2;
    clang_analyzer_pset(p3); // expected-warning {{pset(p3) = i}}
    *p3 = 1;                 // ok
    {
      int j = 0;
      pi = &j;                 // (note: so *ppi now points to j)
      clang_analyzer_pset(pi); // expected-warning {{pset(pi) = j}}
      pi2 = *ppi;
      clang_analyzer_pset(pi2); // expected-warning {{pset(pi2) = j}}
      **ppi = 1;                // ok, modifies j
      *pi2 = 1;                 // ok
      p4 = pi2;
      clang_analyzer_pset(p4); // expected-warning {{pset(p4) = j}}
      *p4 = 1;                 // ok
    }
    clang_analyzer_pset(pi);  // expected-warning {{pset(pi) = (invalid)}}
    clang_analyzer_pset(pi2); // expected-warning {{pset(pi2) = (invalid)}}
    clang_analyzer_pset(p4);  // expected-warning {{pset(p4) = (invalid)}}
  }
  clang_analyzer_pset(p3); // expected-warning {{pset(p3) = (invalid)}}
  clang_analyzer_pset(p2); // expected-warning {{pset(p2) = (invalid)}}
  clang_analyzer_pset(p1); // expected-warning {{pset(p1) = (invalid)}}
}

void Example9() {
  std::vector<int> v1(100);
  clang_analyzer_pset(v1); // expected-warning {{pset(v1) = v1'}}
  int *pi = &v1[0];
  //clang_analyzer_pset(pi); // TODOexpected-warning {{pset(p1) = v1'}}
  auto v2 = std::move(v1);
  //clang_analyzer_pset(pi); // TODOexpected-warning {{pset(p1) = v2'}}
}

void return_pointer() {
  std::vector<int> v1(100);
  clang_analyzer_pset(v1); // expected-warning {{pset(v1) = v1'}}

  int *f(std::vector<int> &);
  int *p = f(v1);
  clang_analyzer_pset(p); // expected-warning {{pset(p) = v1'}}

  int &r = v1[0];
  clang_analyzer_pset(r); // expected-warning {{pset(r) = v1'}}

  int *pmem = v1.data();
  clang_analyzer_pset(pmem); // expected-warning {{pset(pmem) = v1'}}

  auto *v1p = &v1;
  clang_analyzer_pset(v1p); // expected-warning {{pset(v1p) = v1}}

  auto *pmem2 = v1p->data();
  //clang_analyzer_pset(pmem2); // TODOexpected-warning {{pset(pmem2) = v1'}}
}