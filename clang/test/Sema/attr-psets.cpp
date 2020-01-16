// RUN: %clang_cc1 -fcxx-exceptions -fsyntax-only -Wno-undefined-inline -Wno-unused-value -Wno-return-stack-address -Wlifetime -Wlifetime-disabled -Wlifetime-debug -Wlifetime-global -verify -std=c++17 %s

template <typename T>
bool __lifetime_pset(const T &) { return true; }

template <typename T>
bool __lifetime_pset_ref(const T &) { return true; }

template <typename T>
void __lifetime_type_category() {}

namespace std {
class type_info;

template <typename T>
typename T::iterator begin(T &);

template <typename T>
struct general_iterator {
  T &operator*() const;
};

template <typename T>
struct general_const_iterator {
  general_const_iterator() = default;
  general_const_iterator(const general_iterator<T> &);
  T &operator*() const;
};

template <typename T>
struct vector {
  using iterator = general_iterator<T>;
  using const_iterator = general_const_iterator<T>;
  vector(unsigned = 0);
  iterator begin();
  iterator end();
  const_iterator cbegin() const;
  const_iterator cend() const;
  void push_back (const T&);
  const T &operator[](unsigned) const;
  T &operator[](unsigned);
  T &at(unsigned);
  const T &back() const;
  T *data();
  ~vector();
};

template <typename F, typename S>
struct pair {
  F first;
  S second;
};

template <typename T>
struct set {
  using iterator = general_iterator<T>;
  set();
  iterator begin();
  iterator end();
  pair<iterator, bool> insert(const T &);
  template <class... Args>
  pair<iterator, bool> emplace(Args &&... args);
  ~set();
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

template <typename T>
struct basic_string {
    basic_string(const char*);
    basic_string &operator +=(const basic_string& other);
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

template< class T, class... Args >
unique_ptr<T> make_unique( Args&&... args );

template <typename T>
struct optional {
  T &value();
};

template <typename T>
struct list {
  void clear();
  void push_back(const T &);
  T* begin();
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
    __lifetime_pset(p); // expected-warning {{pset(p) = ((*this).m)}}
    int *ps = &s;
    __lifetime_pset(ps); // expected-warning {{pset(ps) = ((static))}}
    int *ps2 = &this->s;
    __lifetime_pset(ps2); // expected-warning {{pset(ps2) = ((static))}}
    __lifetime_pset(mp);  // expected-warning {{pset(mp) = (*(*this).mp)}}
  }
  int *get();
  bool operator==(S s) {
    int *p = s.mp;
    __lifetime_pset(p); // expected-warning {{pset(p) = ((static))}}
    S s2;
    p = s2.mp;
    __lifetime_pset(p); // TODO expected-warning {{pset(p) = ((static))}}
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
  int &operator*();
};

struct [[gsl::Owner]] OwnerOfInt {
  int &operator*();
};

struct [[gsl::Pointer]] PointerToInt {
  int &operator*();
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

  const std::vector<int> &v = std::vector<int>();
  std::vector<int>::const_iterator it = v.cbegin();
  __lifetime_pset(it); // expected-warning {{pset(it) = (*(lifetime-extended temporary through v))}}
}

void rvalue_ref(int &&rref1) {
  __lifetime_pset_ref(rref1); // expected-warning {{pset(rref1) = (*rref1)}}
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

namespace forbidden {
int i;
int *p = &i;

void plus() {
  p++; // expected-warning {{pointer arithmetic disables lifetime analysis}}
}

void pre_plus() {
  ++p;                // expected-warning {{pointer arithmetic disables lifetime analysis}}
}

void minus() {
  p--; // expected-warning {{pointer arithmetic disables lifetime analysis}}
}

void pre_minus() {
  --p; // expected-warning {{pointer arithmetic disables lifetime analysis}}
}

void array_subscript() {
  p[3]; // expected-warning {{pointer arithmetic disables lifetime analysis}}
}
} // namespace forbidden

void array() {
  int a[4];

  int (&ra)[4] = a; // pset(ra) = {a}
  __lifetime_pset_ref(ra); // expected-warning {{(a)}}

  int *p1 = &a[0];
  __lifetime_pset(p1); // expected-warning {{(*a)}}

  int *p2 = a;
  __lifetime_pset(p2); // expected-warning {{(*a)}}

  auto p3 = &a;
  __lifetime_pset(p3); // expected-warning {{(a)}}
}

void pointer_in_array() {
  int *p[4];
  int *k = p[1];
  __lifetime_pset(k); // expected-warning {{pset(k) = (**p)}}
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
  __lifetime_pset(p);  // expected-warning {{((null), *p)}}
  __lifetime_pset(up); // expected-warning {{pset(up) = ((null), *up)}}
  int *alwaysNull = nullptr;
  bool b = p && q;

  if (p) {
    __lifetime_pset(p); // expected-warning {{pset(p) = (*p)}}
  } else {
    __lifetime_pset(p); // expected-warning {{pset(p) = ((null))}}
  }

  if (up) {
    __lifetime_pset(up); // expected-warning {{pset(up) = (*up}}
  } else {
    __lifetime_pset(up); // expected-warning {{pset(up) = ((null))}}
  }

  if (p != nullptr) {
    __lifetime_pset(p); // expected-warning {{pset(p) = (*p)}}
  } else {
    __lifetime_pset(p); // expected-warning {{pset(p) = ((null))}}
  }

  if (p != alwaysNull) {
    __lifetime_pset(p); // expected-warning {{pset(p) = (*p)}}
  } else {
    __lifetime_pset(p); // expected-warning {{pset(p) = ((null))}}
  }

  if (p && __lifetime_pset(p)) // expected-warning {{pset(p) = (*p)}}
    ;

  if (!p || __lifetime_pset(p)) // expected-warning {{pset(p) = (*p)}}
    ;

  p ? __lifetime_pset(p) : false;  // expected-warning {{pset(p) = (*p)}}
  !p ? false : __lifetime_pset(p); // expected-warning {{pset(p) = (*p)}}

  if (!p) {
    __lifetime_pset(p); // expected-warning {{pset(p) = ((null))}}
  } else {
    __lifetime_pset(p); // expected-warning {{pset(p) = (*p)}}
  }

  if (p == nullptr) {
    __lifetime_pset(p); // expected-warning {{pset(p) = ((null))}}
  } else {
    __lifetime_pset(p); // expected-warning {{pset(p) = (*p)}}
  }

  if (p && q) {
    __lifetime_pset(p); // expected-warning {{pset(p) = (*p)}}
    __lifetime_pset(q); // expected-warning {{pset(q) = (*q)}}
  } else {
    __lifetime_pset(p); // expected-warning {{pset(p) = ((null), *p)}}
    __lifetime_pset(q); // expected-warning {{pset(q) = ((null), *q)}}
  }

  if (!p || !q) {
    __lifetime_pset(p); // expected-warning {{pset(p) = ((null), *p)}}
    __lifetime_pset(q); // expected-warning {{pset(q) = ((null), *q)}}
  } else {
    __lifetime_pset(p); // expected-warning {{pset(p) = (*p)}}
    __lifetime_pset(q); // expected-warning {{pset(q) = (*q)}}
  }

  while (p) {
    __lifetime_pset(p); // expected-warning {{pset(p) = (*p)}}
  }

  int i;
  p = &i;
  __lifetime_pset(p); // expected-warning {{(i)}}
  if (p) {
  }
  __lifetime_pset(p); // expected-warning {{(i)}}
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
  (void)&&l1;
}

void cast_to_void() {
  int i;
  void *vptr = &i;    // expected-warning {{unsafe cast disables lifetime analysis}}
}

void cast_from_void(void *vptr) {
  (int *)vptr; // expected-warning {{unsafe cast}}
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

// A more realistic version of assert which has an explicit cast. This explicit
// cast can result in additional basic blocks which makes it harder to handle.
#define assert_explicit(expr) \
  ((static_cast<bool>(expr))  \
       ? static_cast<void>(0) \
       : __assert_fail())

void asserting(const int *p) {
  __lifetime_pset(p); // expected-warning {{pset(p) = ((null), *p)}}
  assert(p);
  __lifetime_pset(p); // expected-warning {{pset(p) = (*p)}}
}

void asserting2(const int *p, const int *q) {
  __lifetime_pset(p); // expected-warning {{pset(p) = ((null), *p)}}
  __lifetime_pset(q); // expected-warning {{pset(q) = ((null), *q)}}
  assert(p && q);
  __lifetime_pset(p); // expected-warning {{pset(p) = (*p)}}
  __lifetime_pset(q); // expected-warning {{pset(q) = (*q)}}
}

void asserting3(const int *p, const int *q) {
  __lifetime_pset(p); // expected-warning {{pset(p) = ((null), *p)}}
  __lifetime_pset(q); // expected-warning {{pset(q) = ((null), *q)}}
  assert(p || q);
  __lifetime_pset(p); // expected-warning {{pset(p) = ((null), *p)}}
  __lifetime_pset(q); // expected-warning {{pset(q) = ((null), *q)}}
}

void asserting4(const int *p) {
  __lifetime_pset(p); // expected-warning {{pset(p) = ((null), *p)}}
  assert_explicit(p);
  __lifetime_pset(p); // expected-warning {{pset(p) = (*p)}}
}

void asserting5(const int *p, const int *q) {
  __lifetime_pset(p); // expected-warning {{pset(p) = ((null), *p)}}
  __lifetime_pset(q); // expected-warning {{pset(q) = ((null), *q)}}
  assert_explicit(p && q);
  __lifetime_pset(p); // expected-warning {{pset(p) = (*p)}}
  __lifetime_pset(q); // expected-warning {{pset(q) = (*q)}}
}

void asserting6(const int *p, const int *q) {
  __lifetime_pset(p); // expected-warning {{pset(p) = ((null), *p)}}
  __lifetime_pset(q); // expected-warning {{pset(q) = ((null), *q)}}
  assert_explicit(p || q);
  __lifetime_pset(p); // expected-warning {{pset(p) = ((null), *p)}}
  __lifetime_pset(q); // expected-warning {{pset(q) = ((null), *q)}}
}

namespace pset_propagation {
struct Bar { int *field; };
void f(const std::vector<Bar>& field_path, bool left_side) {
  const Bar& specific_field = field_path.back();
  __lifetime_pset_ref(specific_field); // expected-warning {{(**field_path)}}
  const int* field = specific_field.field;
  if (field) {
    __lifetime_pset_ref(specific_field); // expected-warning {{(**field_path)}}
  } else {
    if(left_side) {
      __lifetime_pset_ref(specific_field); // expected-warning {{(**field_path)}}
    }
    __lifetime_pset_ref(specific_field); // expected-warning {{(**field_path)}}
  }
}
} // namespace pset_propagation

const int *global_p1 = nullptr;
const int *global_p2 = nullptr;
int global_i;

void namespace_scoped_vars(int param_i, const int *param_p) {
  __lifetime_pset(param_p);   // expected-warning {{pset(param_p) = ((null), *param_p)}}
  __lifetime_pset(global_p1); // expected-warning {{pset(global_p1) = ((static))}}

  if (global_p1) {
    __lifetime_pset(global_p1); // expected-warning {{pset(global_p1) = ((static))}}
    (void)*global_p1;
  }

  int local_i;
  global_p1 = &local_i; // expected-warning {{the pset of '&local_i' must be a subset of {(static), (null)}, but is {(local_i)}}
  global_p1 = &param_i; // expected-warning {{the pset of '&param_i' must be a subset of {(static), (null)}, but is {(param_i)}}
  global_p1 = param_p;  // expected-warning {{the pset of 'param_p' must be a subset of {(static), (null)}, but is {((null), *param_p)}}
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
  // Similar to the f in function_call2 above.
  __lifetime_pset(p); // expected-warning {{pset(p) = ((static))}}
}

void function_call4() {
  PointerToInt f(OwnerOfInt &);

  OwnerOfInt O;
  auto P = f(O);
  __lifetime_pset(P); // expected-warning {{(*O)}}
}

void indirect_function_call() {
  // TODO: indirect calls are not modelled at the moment.
  using F = int *(int *);
  F *f;
  int i = 0;
  int *p = &i;
  int *ret = f(p);
  //__lifetime_pset(p);   // TODOexpected-warning {{pset(p) = (i)}}
  //__lifetime_pset(ret); // TODOexpected-warning {{pset(ret) = (i)}}
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
  __lifetime_pset(p2); // expected-warning {{pset(p2) = (*x)}}
  *p2 = 1;             // ok
  p2 = p;              // D: pset(p2) = pset(p) which is {null}
  __lifetime_pset(p2); // expected-warning {{pset(p2) = ((null))}}
  p2 = &x[10];
  __lifetime_pset(p2); // expected-warning {{pset(p2) = (*x)}}
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
  __lifetime_pset(v1); // expected-warning {{pset(v1) = (*v1)}}
  int *pi = &v1[0];
  __lifetime_pset(pi); // expected-warning {{pset(pi) = (*v1)}}
  pi = &v1.at(0);
  __lifetime_pset(pi); // expected-warning {{pset(pi) = (*v1)}}
  auto v2 = std::move(v1);
  //__lifetime_pset(pi); // TODOexpected-warning {{pset(pi) = (*v2)}}
}

void return_pointer() {
  std::vector<int> v1(100);
  __lifetime_pset(v1); // expected-warning {{pset(v1) = (*v1)}}

  int *f(const std::vector<int> &);
  const std::vector<int> &id(const std::vector<int> &);
  void invalidate(std::vector<int> &);
  int *p = f(v1);
  __lifetime_pset(p); // expected-warning {{pset(p) = (*v1)}}
  const std::vector<int> &v2 = id(v1);
  __lifetime_pset_ref(v2); // expected-warning {{pset(v2) = (v1)}}
  auto it = v1.begin();
  __lifetime_pset(it); // expected-warning {{pset(it) = (*v1)}}
  std::vector<int>::iterator it2;
  it2 = v1.begin();
  __lifetime_pset(it2); // expected-warning {{pset(it2) = (*v1)}}

  int &r = v1[0];
  __lifetime_pset_ref(r); // expected-warning {{pset(r) = (*v1)}}
  __lifetime_pset(p);     // expected-warning {{pset(p) = (*v1)}}

  auto it3 = std::begin(v1);
  int *pmem = v1.data();
  __lifetime_pset(it3);  // expected-warning {{pset(it3) = (*v1)}}
  __lifetime_pset(pmem); // expected-warning {{pset(pmem) = (*v1)}}
  __lifetime_pset(p);    // expected-warning {{pset(p) = (*v1)}}

  auto *v1p = &v1;
  __lifetime_pset(v1p); // expected-warning {{pset(v1p) = (v1)}}

  auto *pmem2 = v1p->data();
  __lifetime_pset(pmem2); // expected-warning {{pset(pmem2) = (*v1)}}
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

void return_not_null_type() {
  int &ret_ref(int *p);
  int *ret_ptr(int *p);
  gsl::not_null<int*> ret_nonnull(int* p);

  int *q = &ret_ref(nullptr);
  __lifetime_pset(q); // expected-warning {{pset(q) = ((static)}}
  q = ret_nonnull(nullptr);
  __lifetime_pset(q); // expected-warning {{pset(q) = ((static))}}
  q = ret_ptr(nullptr);
  __lifetime_pset(q); // expected-warning {{pset(q) = ((null))}}
}

void test_annotations(gsl::nullable<const int *> p,
                      gsl::not_null<const int *> q) {
  __lifetime_pset(p); // expected-warning {{pset(p) = ((null), *p)}}
  __lifetime_pset(q); // expected-warning {{pset(q) = (*q)}}
}

void lifetime_const() {
  class [[gsl::Owner]] Owner {
    int *ptr;

  public:
    int &operator*();
    int *begin() { return ptr; }
    void reset() {}
    [[gsl::lifetime_const]] void peek() {}
  };
  Owner O;
  __lifetime_pset(O); // expected-warning {{pset(O) = (*O)}}
  int *P = O.begin();
  __lifetime_pset(P); // expected-warning {{pset(P) = (*O)}}

  O.peek();
  __lifetime_pset(P); // expected-warning {{pset(P) = (*O)}}

  O.reset();
  __lifetime_pset(P); // expected-warning {{pset(P) = ((invalid))}}
}

void associative_lifetime_const() {
  std::set<int> s;
  std::set<int>::iterator it = s.begin();
  __lifetime_pset(it);  // expected-warning {{pset(it) = (*s)}}
  s.insert(5);
  __lifetime_pset(it);  // expected-warning {{pset(it) = (*s)}}
  s.emplace(5);
  __lifetime_pset(it);  // expected-warning {{pset(it) = (*s)}}
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
  float *q = reinterpret_cast<float *>(p); // expected-warning {{unsafe cast}}
}

void doNotInvalidateReference(std::vector<std::basic_string<char>> v) {
   std::basic_string<char> &r = v[0];
   __lifetime_pset_ref(r); // expected-warning {{(*v)}}
   r += "aa";
   __lifetime_pset_ref(r); // expected-warning {{(*v)}}
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
  __lifetime_pset(O); // expected-warning {{pset(O) = (*O)}}
  int &D = O.value();
  __lifetime_pset_ref(D); // expected-warning {{pset(D) = (*O)}}
  D = 1;

  int &f_ref(const std::optional<int> &O);
  int &D2 = f_ref(O);
  __lifetime_pset_ref(D2); // expected-warning {{pset(D2) = (*O)}}

  int *f_ptr(const std::optional<int> &O);
  int *D3 = f_ptr(O);
  __lifetime_pset(D3); // expected-warning {{pset(D3) = (*O)}}
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
  // TODO: if the input is not null, should we assume the output is never null?
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

void delete_pointee(int *p) {
  int *q = p;
  __lifetime_pset(q); // expected-warning {{((null), *p)}}
  delete q;
  __lifetime_pset(q); // expected-warning {{((invalid))}}
  __lifetime_pset(p); // expected-warning {{((invalid))}}
}

int throw_local() {
  int i;
  // TODO: better error message
  throw &i; // expected-warning {{throwing a pointer with points-to set (i) where points-to set ((static)) is expected}}

  throw; // no argument
}

template <class T>
struct [[gsl::Owner]] OwnerPointsToTemplateType {
  T *get();
  T &operator*();
};

void ownerPointsToTemplateType() {
  OwnerPointsToTemplateType<int> O;
  __lifetime_pset(O); // expected-warning {{pset(O) = (*O)}}
  int *I = O.get();
  __lifetime_pset(I); // expected-warning {{pset(I) = (*O)}}

  // When finding the pointee type of an Owner,
  // look through AutoType to find the ClassTemplateSpecialization.
  auto Oauto = OwnerPointsToTemplateType<int>();
  __lifetime_pset(Oauto); // expected-warning {{pset(Oauto) = (*Oauto)}}
  int *Iauto = Oauto.get();
  __lifetime_pset(Iauto); // expected-warning {{pset(Iauto) = (*Oauto)}}
}

void string_view_ctors(const char *c) {
  std::string_view sv;
  __lifetime_pset(sv); // expected-warning {{pset(sv) = ((null))}}
  std::string_view sv2(c);
  __lifetime_pset(sv2); // expected-warning {{pset(sv2) = ((null), *c)}}
  char local;
  std::string_view sv3(&local, 1);
  __lifetime_pset(sv3); // expected-warning {{pset(sv3) = (local)}}
  std::string_view sv4(sv3);
  __lifetime_pset(sv4); // expected-warning {{pset(sv4) = (local)}}
  //std::string_view sv5(std::move(sv3));
  ///__lifetime_pset(sv5); //TODOexpected-warning {{pset(sv5) = (local)}}
}

void funcptrs() {
  auto fptr = string_view_ctors;
  __lifetime_pset(fptr); // expected-warning {{pset(fptr) = ((static))}}
}

auto lambda_capture(const int *param, const int *param2) {
  const int *&alias = param2;
  auto a = [&]() {
    return *param + *alias;
  };
  //__lifetime_pset(a); // TODOexpected-warning {{pset(a) = (param, param2)}}
  a();
  // TODO: This is a temporary workaround to suppress false positives.
  __lifetime_pset(param);  // expected-warning {{pset(param) = ((unknown))}}
  int i;
  int *ptr = &i;
  auto b = [=]() {
    return *param + *ptr;
  };
  //__lifetime_pset(b); // TODOexpected-warning {{pset(b) = (*param, (null), i)}}
  return b;           // TODOexpected-warning {{returning a pointer with points-to set (*param, (null), i) where points-to set (*param, *param2, (null)) is expected}}
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
  //__lifetime_pset(p); // TODOexpected-warning {{pset(p) = ((static))}}
}

void pruned_branch(bool cond) {
  int i;
  int *trivial = true ? &i : nullptr;
  __lifetime_pset(trivial); // expected-warning {{(i)}}

  int *non_trivial = cond ? &i : nullptr;
  __lifetime_pset(non_trivial); // expected-warning {{((null), i)}}

  // Pruned branches with lvalues
  int a, b;
  int &trivial_r = 0 ? b : a;
  __lifetime_pset_ref(trivial_r); // expected-warning {{(a)}}

  void returns_void();
  0 ? void() : returns_void(); // has not pset, should not crash.

  __lifetime_pset(cond ? OwnerOfInt() : OwnerOfInt()); // expected-warning {{pset(cond ? OwnerOfInt() : OwnerOfInt()) = (*(temporary))}}
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
                     my_pointer &ptr_ref, // expected-note {{it was never initialized here}}
                     my_pointer *ptr_ptr,
                     const my_pointer *ptr_const_ptr) {

  __lifetime_pset(in); // expected-warning {{((null), *in)}}
  assert(in);
  __lifetime_pset(*in); // expected-warning {{((null), **in)}}

  __lifetime_pset_ref(int_ref);       // expected-warning {{(*int_ref)}}
  __lifetime_pset_ref(const_int_ref); // expected-warning {{(*const_int_ref)}}
  __lifetime_pset(owner_by_value);    // expected-warning {{(*owner_by_value)}}

  __lifetime_pset_ref(owner_ref); // expected-warning {{(*owner_ref)}}
  __lifetime_pset(owner_ref);     /// expected-warning {{(**owner_ref)}}

  __lifetime_pset_ref(owner_const_ref); // expected-warning {{(*owner_const_ref)}}
  __lifetime_pset(owner_const_ref);     // expected-warning {{(**owner_const_ref)}}

  __lifetime_pset(ptr_by_value); // expected-warning {{((null), *ptr_by_value)}}

  __lifetime_pset_ref(ptr_const_ref); // expected-warning {{(*ptr_const_ref)}}
  __lifetime_pset(ptr_const_ref);     // expected-warning {{((null), **ptr_const_ref)}}

  __lifetime_pset_ref(ptr_ref); // expected-warning {{(*ptr_ref)}}
  // TODO pending clarification if Pointer& is out or in/out:
  __lifetime_pset(ptr_ref); // expected-warning {{((invalid))}}

  __lifetime_pset(ptr_ptr); // expected-warning {{((null), *ptr_ptr)}}
  assert(ptr_ptr);
  __lifetime_pset(*ptr_ptr); // out: expected-warning {{((invalid))}}

  __lifetime_pset(ptr_const_ptr); // expected-warning {{((null), *ptr_const_ptr)}}
  assert(ptr_const_ptr);
  __lifetime_pset(*ptr_const_ptr); // in: expected-warning {{((null), **ptr_const_ptr)}}
} // expected-warning@-1 {{returning a dangling pointer}}

void foreach_arithmetic() {
  int t[] = {1, 2, 3, 4, 5};
  for (int &i : t) {
    i += 1;
  }
}

void enum_casts() {
  enum EN {
    TEST,
  };
  EN E1, E2;
  ((int &)E1) |= ((int)E2); // expected-warning {{unsafe cast}}
}

void iterator_conversion() {
  std::vector<int> v;
  std::vector<int>::const_iterator it = v.begin();
  __lifetime_pset(it); // expected-warning {{(*v)}}
}

void treatForwardingRefAsLifetimeConst() {
  int x;
  int *p = &x;
  __lifetime_pset(p); // expected-warning {{(x)}}
  std::unique_ptr<int *> up = std::make_unique<int *>(p);
  __lifetime_pset(p); // expected-warning {{(x)}}
}

void fieldOnUpCast() {
  struct Base {};
  struct Derived : public Base {
    int m;
  };
  Base b;
  static_cast<Derived *>(&b)->m = 0;
}

void arrayToPointerDecay() {
  struct {
    int c;
  } e[1];
  int &r = (*e).c;
  __lifetime_pset_ref(r); // expected-warning {{(*e).c}}
}

namespace PointerMembers {
class C {
  C();
  int *p;
  int &r;
  void f() {
    __lifetime_pset_ref(p); // expected-warning {{(*this).p}}
    __lifetime_pset(p); // expected-warning {{(*(*this).p)}}
    __lifetime_pset_ref(r); // expected-warning {{(*(*this).r)}}
  }
};
} // namespace PointerMembers

namespace CXXScalarValueInitExpr {
template <typename a>
class b {
public:
  void c() {
    // CXXScalarValueInitExpr -> value-initialization
    int *p = a();
    __lifetime_pset(p); // expected-warning {{((null))}}
  }
};

void d() {
  b<int *> d;
  d.c(); // expected-note {{in instantiation}}
}
} // namespace CXXScalarValueInitExpr

namespace PointerToMember {
struct Aggregate {
  int *p1;
  int *p2;
};

void f() {
  Aggregate a;
  int i;
  a.p1 = &i; // make a.p1 valid; a.p2 still invalid
  int *Aggregate::*memptr = &Aggregate::p2;
  (void)*(a.*memptr); // expected-warning {{pointer arithmetic disables lifetime analysis}}
}
} // namespace PointerToMember

namespace SubstNonTypeTemplateParmExpr {
template <int *i>
void f() {
  (void)*i; // Template parameters have pset (static)
}
int g;
void test() {
  f<&g>();
}
} // namespace SubstNonTypeTemplateParmExpr

namespace Aggregates {
struct UnscannedEntry {
  std::vector<int> V1;
  std::vector<int> V2;
};
std::vector<int> get();

void f() {
  UnscannedEntry E;
  __lifetime_type_category<UnscannedEntry>(); // expected-warning {{Aggregate}}
  // We don't handle Aggregates yet
  __lifetime_pset(E.V1); // expected-warning {{((static)}}
}

struct InClassInitializer {
  int a;
  const char *mem = nullptr;
};

void g() {
  InClassInitializer c{3};
  __lifetime_pset(c.mem); // expected-warning {{((static)}}
}
} // namespace Aggregates

namespace TypeId {
struct A {};
void f(const std::type_info &);

void g() {
  f(typeid(A));
}
} // namespace TypeId

namespace vaargs {
void f(int num, ...) {
  __builtin_va_list list;
  __builtin_va_start(list, num);
  int *p = __builtin_va_arg(list, int*);
  __builtin_va_end(list);
}
} // namespace vaargs

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
  int &operator*();
};
void b() {
  a c;
  c = 0;
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

namespace creduce8 {
// ImplicitValueInitExpr of array type
class a {
  char b[];
  a();
};
a::a() : b() {}
} // namespace creduce8

namespace creduce9 {
template <typename a>
struct b : a {};
template <typename = b<int>>
class c {};
class B {
  // b<int> is ill-formed, but only used by name
  // We don't want to see any errors when we internally
  // try to look at the instantiation.
  c<> d;
  B() {}
};
} // namespace creduce9

namespace creduce10 {
// Don't crash when 'begin' is a field instead of a member function
class a {
  char begin;
};
void b(a) {}
} // namespace creduce10

namespace creduce11 {
template <typename a>
struct b {
  a operator[](long);
};
struct g {};
class f {
  b<g> h;
  // g is an Aggregate temporary, and it needs to have a pset
  // because it is passed as r-value reference to the implicit operator=.
  void i() { h[0] = g(); }
};
} // namespace creduce11

namespace creduce12 {
class a {
public:
  void operator*();
  a operator++(int);
};
class b {
public:
  using c = a;
  template <class d>
  void e(c f, d) { *f++; }
};
class g {
  using h = b;
  h i;
  void k() {
    a j;
    i.e(j, this);
  }
};
} // namespace creduce12

namespace creduce13 {
template <typename a>
a b(a &&);

template <typename c>
struct d {};

class e {
  d<int> operator*();
};

class h {
  e begin();
  void end();
};

class j {
  // k is an output paramter (because h is a Pointer)
  j(h &&k) { // expected-note 2 {{it was never initialized here}}
     __lifetime_pset_ref(k); // expected-warning {{pset(k) = (*k)}}
    b(k); // expected-warning {{passing a dangling pointer as argument}}
    return;  // expected-warning {{returning a dangling pointer as output value '*k'}}
  }
};
} // namespace creduce13

namespace creduce14 {
// With this bug, c was classified as Aggregate,
// but a<int> as Pointer (due to the correct methods
// and PointeeType from template parameter.)
// The d.b() crashed, because b expected *this to have a
// pset, but the Aggregate d in fn1 does not have one.
// Now c is also classified as Pointer.
template <typename>
class a {
public:
  void begin();
  void end();
  void b();
};

class c : public a<int> {};

void fn1(c d) {
  d.b();
}
} // namespace creduce14
namespace bug_report_66 {
class [[gsl::Owner]] O {
public:
  void a();
  int *begin();
};

void working() {
  O o;
  auto &R = o;
  // R points _at_ o
  __lifetime_pset_ref(R); // expected-warning {{pset(R) = (o)}}

  auto *iter = R.begin();
  // iter now points _into_ o
  __lifetime_pset(iter);  // expected-warning {{pset(iter) = (*o)}}
  __lifetime_pset_ref(R); // expected-warning {{pset(R) = (o)}}

  R.a(); // invalidates psets that contain *o.
  // does not invalidate psets that contain o.

  __lifetime_pset_ref(R); // expected-warning {{pset(R) = (o)}}
  __lifetime_pset(iter);  // expected-warning {{pset(iter) = ((invalid))}}
}

void not_working(O &R) {
  __lifetime_pset_ref(R); // expected-warning {{pset(R) = (*R)}}
  __lifetime_pset(R);     // expected-warning {{pset(R) = (**R)}}

  auto *iter = R.begin();
  __lifetime_pset(iter);  // expected-warning {{pset(iter) = (**R)}}
  __lifetime_pset_ref(R); // expected-warning {{pset(R) = (*R)}}

  // Should invalidate psets that contain **R.
  // Should not invalidate psets that contain *R.
  R.a();
  __lifetime_pset_ref(R); // expected-warning {{pset(R) = (*R)}}
  __lifetime_pset(iter);  // expected-warning {{pset(iter) = ((invalid))}}
}
} // namespace bug_report_66
namespace owner_in_owner_invalidation {
auto fun() {
  std::vector<std::vector<int>> v1;
  std::vector<int> &v2 = *v1.begin();
  auto *p1 = &v1;
  auto *p2 = &v2;
  auto i1 = v1.begin();
  auto i2 = v2.begin();
  v2.push_back(1);
  __lifetime_pset(v2);  // expected-warning {{pset(v2) = ((invalid))}}
  __lifetime_pset(p1);  // expected-warning {{pset(p1) = (v1)}}
  __lifetime_pset(p2);  // expected-warning {{pset(p2) = (*v1)}}
  __lifetime_pset(i1);  // expected-warning {{pset(i1) = (*v1)}}
  __lifetime_pset(i2);  // expected-warning {{pset(i2) = ((invalid))}}
  i2 = v2.begin();
  v1.push_back({});     // expected-note {{modified here}}
  __lifetime_pset(v2);  // expected-warning {{pset(v2) = ((unknown))}}
                        // expected-warning@-1 {{dereferencing a dangling pointer}}
  __lifetime_pset(p1);  // expected-warning {{pset(p1) = (v1)}}
  __lifetime_pset(p2);  // expected-warning {{pset(p2) = ((invalid))}}
  __lifetime_pset(i1);  // expected-warning {{pset(i1) = ((invalid))}}
  __lifetime_pset(i2);  // expected-warning {{pset(i2) = ((invalid))}}
}
} // namespace owner_in_owner_invalidation
namespace bug_report_69 {
const int &min(const int &a, const int &b); //like std::min
class A {
public:
  const int &b() const;
};
const int &A::b() const { return 0; } // expected-warning {{returning a dangling pointer}}
// expected-note@-1 {{temporary was destroyed at the end of the full expression}}
auto fun() {
  auto &m = min(1, 2);  // expected-note {{temporary was destroyed at the end of the full expression}}
  __lifetime_pset_ref(m); // expected-warning {{pset(m) = ((unknown))}}
  // expected-warning@-1 {{dereferencing a dangling pointer}}
}

} // namespace bug_report_69

namespace bug_report_86 {
struct A {
  unsigned data[2];
};

A f(unsigned start, unsigned end) {
    return {start, end};
}
} // namespace bug_report_86

namespace expressions_statement {
void f() {
  __lifetime_pset(({ int *p = 0; p;})); // expected-warning {{((unknown))}}
}
} // namespace expressions_statement

namespace structured_bindings {
struct A { int *a; int *b; };
A getA();

void f() {
  auto [a, b] = getA();
  // TODO: fix these
  __lifetime_pset(a); // expected-warning {{((unknown))}}
  __lifetime_pset(b); // expected-warning {{((unknown))}}
}
} // namespace structured_bindings

namespace linked_structures {
struct Node {
  Node *next;
  void freeList() {
    Node *cur = this;
    while (cur) {
      __lifetime_pset(cur); // expected-warning {{(*this)}}
                            // expected-warning@-1 {{(*(*this).next, *this)}}
                            // expected-warning@-2 {{((static), *(*this).next, *this)}}
      Node *next_temp = cur->next;
      __lifetime_pset(next_temp); // expected-warning {{(*(*this).next)}}
                                  // expected-warning@-1 {{((static), *(*this).next)}}
                                  // expected-warning@-2 {{((static), *(*this).next)}}
      delete cur;
      __lifetime_pset(next_temp); // expected-warning {{(*(*this).next)}}
                                  // expected-warning@-1 {{((static), *(*this).next)}}
                                  // expected-warning@-2 {{((static), *(*this).next)}}
      __lifetime_pset(cur); // expected-warning {{(invalid)}}
                            // expected-warning@-1 {{(invalid)}}
                            // expected-warning@-2 {{(invalid)}}
      cur = next_temp;
    }
  }
};

namespace boundedness {
struct Node;
struct [[gsl::Owner]] SubRegion {
  Node *begin();
};
bool cond();
struct Node {
  SubRegion subregions_;
  Node *parent_;
  bool f() {
    for (auto itr = subregions_.begin(); cond();) {
      Node* up = itr->parent_;
      __lifetime_pset(up); // expected-warning {{(*(*(*this).subregions_).parent_)}}

      do {
        up = up->subregions_.begin();
        up = up->parent_;
        __lifetime_pset(up); // expected-warning {{(*(*(*(*(*this).subregions_).parent_).subregions_).parent_)}}
                             // expected-warning@-1 {{((static), *(*(*(*(*this).subregions_).parent_).subregions_).parent_)}}
                             // expected-warning@-2 {{((static), *(*(*(*(*this).subregions_).parent_).subregions_).parent_)}}
      } while (cond());
    }
    return true;
  }
};
} // namespace boundedness


void std_list_invalidation() {
  std::list<int> l;
  auto begin = l.begin();
  __lifetime_pset(begin); // expected-warning {{(*l)}}
  l.push_back(5);
  __lifetime_pset(begin); // expected-warning {{(*l)}}
  l.clear();
  __lifetime_pset(begin); // expected-warning {{((invalid))}}
}
} // namespace linked_structures
