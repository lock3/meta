// RUN: %clang_cc1 -fcxx-exceptions -fsyntax-only -Wlifetime -Wlifetime-debug -verify %s

template <typename T>
bool __lifetime_pset(const T &) { return true; }

template <typename T>
bool __lifetime_pset_ref(const T &) { return true; }

namespace std {
template <typename T>
typename T::iterator begin(T &);

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

template <typename T>
struct basic_string_view {
  basic_string_view();
  basic_string_view(const T *);
  basic_string_view(const T *, unsigned);
  const T *begin();
  const T *end();
};

using string_view = basic_string_view<char>;

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
  bool operator==(S s) {
    int *p = s.mp;
    __lifetime_pset(p); // expected-warning {{pset(p) = ((static))}}
    S s2;
    p = s2.mp;
    __lifetime_pset(p); // expected-warning {{pset(p) = ((invalid))}}
    const S &s3 = S();
    p = s3.mp;
    __lifetime_pset(p); // expected-warning {{pset(p) = ((static))}}
    return true;
  }

  // Crash reproduced with convoluted CFG.
  void foorbar(const S &s, bool b) {
    int *p = nullptr;
    if (s.mp) {
      if (!mp)
        p = nullptr;
      p = s.mp;
    }
  }
};

struct D : public S {
  ~D();
};

struct [[gsl::Pointer]] my_pointer {
  my_pointer();
  my_pointer &operator=(const my_pointer &);
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
  p = p ?: nullptr;
  __lifetime_pset(p); // expected-warning {{pset(p) = ((null), i)}}
  const char *text = __PRETTY_FUNCTION__;
  __lifetime_pset(text); // expected-warning {{pset(text) = ((static))}}
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

void if_stmt(const int *p, const char *q,
             gsl::nullable<std::unique_ptr<int>> up) {
  __lifetime_pset(p);  // expected-warning {{((*p), (null))}}
  __lifetime_pset(up); // expected-warning {{pset(up) = ((null), up')}}
  int *alwaysNull = nullptr;
  bool b = p && q;

  if (p) {
    __lifetime_pset(p); // expected-warning {{pset(p) = ((*p))}}
  } else {
    __lifetime_pset(p); // expected-warning {{pset(p) = ((null))}}
  }

  if (up) {
    __lifetime_pset(up); // expected-warning {{pset(up) = (up')}}
  } else {
    __lifetime_pset(up); // expected-warning {{pset(up) = ((null))}}
  }

  if (p != nullptr) {
    __lifetime_pset(p); // expected-warning {{pset(p) = ((*p))}}
  } else {
    __lifetime_pset(p); // expected-warning {{pset(p) = ((null))}}
  }

  if (p != alwaysNull) {
    __lifetime_pset(p); // expected-warning {{pset(p) = ((*p))}}
  } else {
    __lifetime_pset(p); // expected-warning {{pset(p) = ((null))}}
  }

  if (p && __lifetime_pset(p)) // expected-warning {{pset(p) = ((*p))}}
    ;

  if (!p || __lifetime_pset(p)) // expected-warning {{pset(p) = ((*p))}}
    ;

  p ? __lifetime_pset(p) : false;  // expected-warning {{pset(p) = ((*p))}}
  !p ? false : __lifetime_pset(p); // expected-warning {{pset(p) = ((*p))}}

  if (!p) {
    __lifetime_pset(p); // expected-warning {{pset(p) = ((null))}}
  } else {
    __lifetime_pset(p); // expected-warning {{pset(p) = ((*p))}}
  }

  if (p == nullptr) {
    __lifetime_pset(p); // expected-warning {{pset(p) = ((null))}}
  } else {
    __lifetime_pset(p); // expected-warning {{pset(p) = ((*p))}}
  }

  if (p && q) {
    __lifetime_pset(p); // expected-warning {{pset(p) = ((*p))}}
    __lifetime_pset(q); // expected-warning {{pset(q) = ((*q))}}
  } else {
    __lifetime_pset(p); // expected-warning {{pset(p) = ((*p), (null)}}
    __lifetime_pset(q); // expected-warning {{pset(q) = ((*q), (null))}}
  }

  if (!p || !q) {
    __lifetime_pset(p); // expected-warning {{pset(p) = ((*p), (null))}}
    __lifetime_pset(q); // expected-warning {{pset(q) = ((*q), (null))}}
  } else {
    __lifetime_pset(p); // expected-warning {{pset(p) = ((*p))}}
    __lifetime_pset(q); // expected-warning {{pset(q) = ((*q))}}
  }

  while (p) {
    __lifetime_pset(p); // expected-warning {{pset(p) = ((*p))}}
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

void condition_short_circuit(const S *p) {
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
  __lifetime_pset(p); // expected-warning {{pset(p) = (i, initial, j)}}
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
  void *ptr = &&l1;
  __lifetime_pset(ptr); // expected-warning {{pset(ptr) = ((static))}}
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
  int j;
  goto e;
  int *p;
e:;
  // We do not care about this case since the core guidelines forbid
  // the use of goto.
  __lifetime_pset(p); // expected-warning {{pset(p) = ((invalid))}}
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

void asserting(const int *p) {
  __lifetime_pset(p); // expected-warning {{pset(p) = ((*p), (null))}}
  assert(p);
  __lifetime_pset(p); // expected-warning {{pset(p) = ((*p))}}
}

const int *global_p1 = nullptr;
const int *global_p2 = nullptr;
int global_i;

void namespace_scoped_vars(int param_i, const int *param_p) {
  __lifetime_pset(param_p);   // expected-warning {{pset(param_p) = ((*param_p), (null))}}
  __lifetime_pset(global_p1); // expected-warning {{pset(global_p1) = ((static))}}

  if (global_p1) {
    __lifetime_pset(global_p1); // expected-warning {{pset(global_p1) = ((static))}}
    (void)*global_p1;
  }

  int local_i;
  global_p1 = &local_i; // expected-warning {{the pset of '&local_i' must be a subset of {(static), (null)}, but is {(local_i)}}
  global_p1 = &param_i; // expected-warning {{the pset of '&param_i' must be a subset of {(static), (null)}, but is {(param_i)}}
  global_p1 = param_p;  // expected-warning {{the pset of 'param_p' must be a subset of {(static), (null)}, but is {((*param_p), (null))}}
  const int *local_p = global_p1;
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
  // The deref location of the argument is an output only,
  // the the function has no input with matching type.
  __lifetime_pset(p); // expected-warning {{pset(p) = ((static))}}
}

void function_call3() {
  void f(int *&p);

  int i;
  int *p = &i;
  __lifetime_pset(p); // expected-warning {{pset(p) = (i)}}
  f(p);
  __lifetime_pset(p); // expected-warning {{pset(p) = ((static))}}
}

void indirect_function_call() {
  using F = int *(int *);
  F *f;
  int i = 0;
  int *p = &i;
  int *ret = f(p);
  __lifetime_pset(p);   // expected-warning {{pset(p) = (i)}}
  __lifetime_pset(ret); // expected-warning {{pset(ret) = (i)}}
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
  __lifetime_pset_ref(good);   // expected-warning {{pset(good) = (x, y)}}

  const int &bad = min(x, y + 1);
  // expected-note@-1 {{temporary was destroyed at the end of the full expression}}
  __lifetime_pset_ref(bad); // expected-warning {{pset(bad) = ((unknown))}}
                            // expected-warning@-1 {{dereferencing a dangling pointer}}
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
  std::vector<int>::iterator it2;
  it2 = v1.begin();
  __lifetime_pset(it2); // expected-warning {{pset(it2) = (v1')}}

  int &r = v1[0];
  __lifetime_pset_ref(r); // expected-warning {{pset(r) = (v1')}}
  __lifetime_pset(p);     // expected-warning {{pset(p) = (v1')}}

  auto it3 = std::begin(v1);
  int *pmem = v1.data();
  __lifetime_pset(it3);  // expected-warning {{pset(it3) = (v1')}}
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

void test_annotations(gsl::nullable<const int *> p,
                      gsl::not_null<const int *> q) {
  __lifetime_pset(p); // expected-warning {{pset(p) = ((*p), (null))}}
  __lifetime_pset(q); // expected-warning {{pset(q) = ((*q))}}
}

void lifetime_const() {
  class [[gsl::Owner]] Owner {
    int *ptr;

  public:
    int operator*();
    int *begin() { return ptr; }
    void reset() {}
    [[gsl::lifetime_const]] void peek() {}
  };
  Owner O;
  __lifetime_pset(O); // expected-warning {{pset(O) = (O')}}
  int *P = O.begin();
  __lifetime_pset(P); // expected-warning {{pset(P) = (O')}}

  O.peek();
  __lifetime_pset(P); // expected-warning {{pset(P) = (O')}}

  O.reset();
  __lifetime_pset(P); // expected-warning {{pset(P) = ((invalid))}}
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
  __lifetime_pset(p1); // expected-warning {{pset(p1) = (w, z)}}
  __lifetime_pset(p2); // expected-warning {{pset(p2) = (y, z)}}
  __lifetime_pset(pp); // expected-warning {{pset(pp) = (p1, p2)}}
}

void cast(int *p) {
  float *q = reinterpret_cast<float *>(p);
  __lifetime_pset(q); // expected-warning {{pset(q) = ((invalid))}}
}

// Support CXXOperatorCallExpr on non-member function
struct S3;
void operator==(const S3 &A, const S3 &B) { A == B; }

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
  __lifetime_pset_ref(D); // expected-warning {{pset(D) = (O')}}
  D = 1;

  int &f_ref(const std::optional<int> &O);
  int &D2 = f_ref(O);
  __lifetime_pset_ref(D2); // expected-warning {{pset(D2) = (O')}}

  int *f_ptr(const std::optional<int> &O);
  int *D3 = f_ptr(O);
  __lifetime_pset(D3); // expected-warning {{pset(D3) = (O')}}
}

my_pointer global_pointer;
void f(my_pointer &p) { // expected-note {{it was never initialized here}}
  // p is a out-parameter, so its pset is {invalid}
  (void)*p;           // expected-warning {{passing a dangling pointer as argument}}
  p = global_pointer; // OK, *this is not validated on assignment operator call
}
void caller() {
  void f(my_pointer & p);
  my_pointer p;
  f(p); // OK, p is assumed to be out-parameter, so no validation
  my_pointer p2;
  p2 = global_pointer;
}

struct Struct {
  Struct *f();
  Struct &g();
};

void this_in_input() {
  Struct S;
  Struct *Sptr = &S;
  Struct *ptr = S.f();
  __lifetime_pset(ptr); // expected-warning {{pset(ptr) = (S)}}
  ptr = Sptr->f();
  __lifetime_pset(ptr); // expected-warning {{pset(ptr) = (S)}}

  Struct &Sref = S.g();
  __lifetime_pset_ref(Sref); // expected-warning {{pset(Sref) = (S)}}

  Struct &Sref2 = Sptr->g();
  __lifetime_pset_ref(Sref2); // expected-warning {{pset(Sref2) = (S)}}
}
void derived_to_base_conversion() {
  S *f(D *);
  D d;
  S *sp = f(&d);
  __lifetime_pset(sp); // expected-warning {{pset(sp) = (d)}}
}

void kill_materialized_temporary() {
  const int *p;
  {
    const int &i = 1;
    __lifetime_pset_ref(i); // expected-warning {{pset(i) = ((lifetime-extended temporary through i))}}
    p = &i;
    __lifetime_pset(p); // expected-warning {{pset(p) = ((lifetime-extended temporary through i))}}
  }
  __lifetime_pset(p); // expected-warning {{pset(p) = ((invalid))}}
}

int throw_local() {
  int i;
  // TODO: better error message
  throw &i; // expected-warning {{throwing a pointer with points-to set (i) where points-to set ((static)) is expected}}
}

template <class T>
struct [[gsl::Owner]] OwnerPointsToTemplateType {
  T *get();
};

void ownerPointsToTemplateType() {
  OwnerPointsToTemplateType<int> O;
  __lifetime_pset(O); // expected-warning {{pset(O) = (O'}}
  int *I = O.get();
  __lifetime_pset(I); // expected-warning {{pset(I) = (O'}}

  // When finding the pointee type of an Owner,
  // look through AutoType to find the ClassTemplateSpecialization.
  auto Oauto = OwnerPointsToTemplateType<int>();
  __lifetime_pset(Oauto); // expected-warning {{pset(Oauto) = (Oauto')}}
  int *Iauto = Oauto.get();
  __lifetime_pset(Iauto); // expected-warning {{pset(Iauto) = (Oauto')}}
}

void string_view_ctors(const char *c) {
  std::string_view sv;
  __lifetime_pset(sv); // expected-warning {{pset(sv) = ((null))}}
  std::string_view sv2(c);
  __lifetime_pset(sv2); // expected-warning {{pset(sv2) = ((*c), (null))}}
  char local;
  std::string_view sv3(&local, 1);
  __lifetime_pset(sv3); // expected-warning {{pset(sv3) = (local)}}
  std::string_view sv4(sv3);
  __lifetime_pset(sv4); // expected-warning {{pset(sv4) = (local)}}
  //std::string_view sv5(std::move(sv3));
  ///__lifetime_pset(sv5); //TODOexpected-warning {{pset(sv5) = (local)}}
}

void unary_operator(const char *p) {
  const char *q = --p;
  __lifetime_pset(p); // expected-warning {{pset(p) = ((invalid))}}
  __lifetime_pset(q); // expected-warning {{pset(q) = ((invalid))}}
}

void funcptrs() {
  auto fptr = unary_operator;
  __lifetime_pset(fptr); // expected-warning {{pset(fptr) = ((static))}}
}

auto lambda_capture(const int *param, const int *param2) {
  const int *&alias = param2;
  auto a = [&]() {
    return *param + *alias;
  };
  __lifetime_pset(a); // TODOexpected-warning {{pset(a) = (param, param2)}}
  int i;
  int *ptr = &i;
  auto b = [=]() {
    return *param + *ptr;
  };
  __lifetime_pset(b); // TODOexpected-warning {{pset(b) = ((*param), (null), i)}}
  return b;           // TODOexpected-warning {{returning a Pointer with points-to set ((*param), (null), i) where points-to set ((*param), (*param2), (null)) is expected}}
}

typedef int T;
void f(int *p) {
  p->T::~T();
}

void default_argument() {
  int *null(int *p = nullptr);
  int *staticf(int *p = &global_i);

  int *p = null();
  //__lifetime_pset(p); //TODOexpected-warning {{pset(p) = ((null))}}

  p = staticf();
  __lifetime_pset(p); // expected-warning {{pset(p) = ((static))}}
}

void pruned_branch(bool cond) {
  int i;
  int *trivial = true ? &i : nullptr;
  __lifetime_pset(trivial); // expected-warning {{(i)}}

  int *non_trivial = cond ? &i : nullptr;
  __lifetime_pset(non_trivial); // expected-warning {{((null), i)}}
}

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
                     const my_pointer *ptr_const_ptr) {

  __lifetime_pset(in); // expected-warning {{((*in), (null))}}
  assert(in);
  __lifetime_pset(*in); // expected-warning {{((null), (static))}}

  __lifetime_pset_ref(int_ref);       // expected-warning {{((*int_ref))}}
  __lifetime_pset_ref(const_int_ref); // expected-warning {{((*const_int_ref))}}
  __lifetime_pset(owner_by_value);    // expected-warning {{(owner_by_value')}}

  __lifetime_pset_ref(owner_ref); // expected-warning {{((*owner_ref))}}
  __lifetime_pset(owner_ref);     /// expected-warning {{((*owner_ref)')}}

  __lifetime_pset_ref(owner_const_ref); // expected-warning {{((*owner_const_ref))}}
  __lifetime_pset(owner_const_ref);     // expected-warning {{((*owner_const_ref)')}}

  __lifetime_pset(ptr_by_value); // expected-warning {{((*ptr_by_value), (null))}}

  __lifetime_pset_ref(ptr_const_ref); // expected-warning {{((*ptr_const_ref))}}
  __lifetime_pset(ptr_const_ref);     // expected-warning {{((static))}} TODO correct?

  __lifetime_pset_ref(ptr_ref); // expected-warning {{((*ptr_ref))}}
  // TODO pending clarification if Pointer& is out or in/out:
  __lifetime_pset(ptr_ref); // expected-warning {{((invalid))}}

  __lifetime_pset(ptr_ptr); // expected-warning {{((*ptr_ptr), (null))}}
  assert(ptr_ptr);
  __lifetime_pset(*ptr_ptr); // out: expected-warning {{((invalid))}}

  __lifetime_pset(ptr_const_ptr); // expected-warning {{((*ptr_const_ptr), (null))}}
  assert(ptr_const_ptr);
  __lifetime_pset(*ptr_const_ptr); // in: expected-warning {{((null), (static))}}
}

void foreach_arithmetic() {
  int t[] = {1, 2, 3, 4, 5};
  for (int &i : t) {
    i += 1;
  }
}

namespace crashes {
// This used to crash with missing pset.
// It's mainly about knowing if the first argument
// of a CXXOperatorCall is the this pointer or not.
class b {
public:
  b(int);
  void operator*();
};
template <typename a>
void operator!=(const b &, const a &);
struct {
  b c = 0;
} e;
void d() { e.c != 0; }

void ignore_casts() {
  (void)bool(nullptr);
  (void)bool(1);
  unsigned d = 0;
}
} // namespace crashes

namespace creduce2 {
enum a { b };
class c {
public:
  c(a);
  void operator*();
};
void d() { c e = b; }
} // namespace creduce2

namespace creduce3 {
class a {
  void b() {
    a c;
    m_fn2() == c;
  }
  void operator==(a);
  a m_fn2();
};
} // namespace creduce3

namespace creduce4 {
class a {
public:
  void operator=(int);
  void operator*();
};
void b() {
  a c;   // expected-note {{default-constructed Pointers are assumed to be null}}
  c = 0; // expected-warning {{passing a null pointer as argument to a non-null parameter}}
}
} // namespace creduce4

namespace creduce5 {
class a {
  long b;
  a &operator+=(a);
};
a &a::operator+=(a) {
  b += b;
  return *this;
}
} // namespace creduce5

namespace creduce6 {
class a {
protected:
  static void b();
};
class c : a {
  c() { (void)this->b; }
};
} // namespace creduce6

namespace creduce7 {
int a;
void b() {
  int c = ((void)b, a);
  (void)c;
}
} // namespace creduce7