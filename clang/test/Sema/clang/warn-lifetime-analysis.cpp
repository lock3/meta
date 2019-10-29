// RUN: %clang_cc1 -fsyntax-only -verify -Wlifetime %s
namespace std {
using size_t = decltype(sizeof(int));

struct string_view {
  struct iterator {
    char &operator*();
    iterator &operator++();
    bool operator!=(const iterator &) const;
  };
  string_view();
  string_view(const char *s);
  char &operator[](int i);
  iterator begin();
  iterator end();
};

struct string {
  struct iterator {
    char &operator*();
    iterator &operator++();
    bool operator!=(const iterator &) const;
  };
  string();
  string(const char *s, size_t len);
  string(const char *s);
  explicit string(const string_view &);
  char &operator[](int i);
  operator string_view();
  ~string();
  iterator begin();
  iterator end();
};

template <typename T>
struct unique_ptr {
  T &operator*() const;
  T *get();
  ~unique_ptr();
};

template <class T, class... Args>
unique_ptr<T> make_unique(Args &&... args);

template <typename T>
struct optional {
  T &value();
};

template <class T>
struct allocator {
  allocator();
};

template <class T>
class initializer_list {
  initializer_list() noexcept;
};

template <
    class T,
    class Allocator = std::allocator<T>>
struct vector {
  struct iterator {
    T &operator*();
    iterator &operator++();
    bool operator!=(const iterator &) const;
  };
  vector(size_t);
  vector(std::initializer_list<T> init,
         const Allocator &alloc = Allocator());
  T &operator[](size_t);
  iterator begin();
  iterator end();
  ~vector();
};

template <typename K, typename V>
struct pair {
  K first;
  V second;
};

template <typename K, typename V>
struct map {
  using iterator = pair<K, V> *;
  iterator find(const K &) const;
  iterator end() const;
};
} // namespace std

struct Owner {
  ~Owner();
  int m;
  int f();
};

struct [[gsl::Pointer]] my_pointer {
  int &operator*();
};

void deref_uninitialized() {
  int *p;        // expected-note {{it was never initialized here}}
  *p = 3;        // expected-warning {{dereferencing a dangling pointer}}
  my_pointer p2; // expected-note {{default-constructed Pointers are assumed to be null}}
  *p2;           // expected-warning {{passing a null pointer as argument to a non-null parameter}}
}

void deref_nullptr() {
  int *q = nullptr; // expected-note {{assigned here}}
  *q = 3;           // expected-warning {{dereferencing a null pointer}}
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
  Owner *p;
  {
    Owner s;
    p = &s;
    p->f();     // OK
  }             // expected-note 3 {{pointee 's' left the scope here}}
  p->f();       // expected-warning {{passing a dangling pointer as argument}}
  int i = p->m; // expected-warning {{dereferencing a dangling pointer}}
  p->m = 4;     // expected-warning {{dereferencing a dangling pointer}}
}

// No Pointer involved, thus not checked.
void ignore_access_on_non_ref_ptr() {
  Owner s;
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
  g(q); // OK, *q is invalid but out parameter

  int i;
  p = &i;
  h(p, q);
}

void for_stmt() {
  int *p; // expected-note {{it was never initialized here}}
  for (int i = 0; i < 1024; ++i) {
    (void)*p; // expected-warning {{dereferencing a dangling pointer}}
    int j;
    p = &j;
  }
}

std::string operator"" _s(const char *str, std::size_t len);

void do_not_warn_for_decay_only() {
  auto str = "decaythis"_s;
}

const int *return_wrong_ptr(const int *p) {
  int i = 0;
  int *q = &i;
  if (p)
    return p;
  return q; // expected-warning {{returning a dangling Pointer}}
  // expected-note@-1 {{pointee 'i' left the scope here}}
}

void null_notes(int *p) {
  // expected-note@-1 2 {{the parameter is assumed to be potentially null. Consider using gsl::not_null<>, a reference instead of a pointer or an assert() to explicitly remove null}}
  (void)*p; // expected-warning {{dereferencing a possibly null pointer}}

  if (p) {
    (void)*p;
  } else {
    (void)*p; // expected-warning {{dereferencing a null pointer}}
  }
}

void null_notes_copy(int *p) {
  // expected-note@-1 {{the parameter is assumed to be potentially null}}
  int *q = p; // expected-note {{assigned here}}
  (void)*q;   // expected-warning {{dereferencing a possibly null pointer}}
}

void null_notes_copy2(int *p) {
  // expected-note@-1 {{the parameter is assumed to be potentially null}}
  int *q;
  q = p;    // expected-note {{assigned here}}
  (void)*q; // expected-warning {{dereferencing a possibly null pointer}}
}

namespace supress_further_warnings {
int *f(int *);
void test() {
  int *p;        // expected-note {{it was never initialized here}}
  int *q = f(p); // expected-warning {{passing a dangling pointer as argument}}
  (void)*q;      // further diagnostics are suppressed here
}
} // namespace supress_further_warnings

namespace do_not_check_Owner_methods {
struct [[gsl::Owner]] Owner {
  int &operator*();
  ~Owner();
  void f() {
    int *i;
    (void)*i;
  }
};
} // namespace do_not_check_Owner_methods

int &f(int &a) {
  return a;
}
int &hello() {
  int x = 0;
  return f(x); // expected-warning {{dangling}} expected-note {{pointee 'x' left}}
}

// Examples from paper P0936 by Richard Smith and Nicolai Josuttis
namespace P0936 {
template <typename T>
void use(const T &);

void sj2() {
  char &c = std::string{"my non-sso string"}[0];
  // expected-note@-1 {{temporary was destroyed at the end of the full expression}}
  c = 'x'; // expected-warning {{dereferencing a dangling pointer}}
}

void sj2_alt() {
  char *c = std::make_unique<char>().get();
  // expected-note@-1 {{temporary was destroyed at the end of the full expression}}
  *c = 'x'; // expected-warning {{dereferencing a dangling pointer}}
}

std::vector<int> getVec();
std::optional<std::vector<int>> getOptVec();

void sj3() {
  for (int value : getVec()) { // OK
  }

  for (int value : getOptVec().value()) { // expected-warning {{dereferencing a dangling pointer}}
    // expected-note@-1 {{temporary was destroyed at the end of the full expression}}
  }
}

std::vector<int> getVec_alt();
std::unique_ptr<std::vector<int>> getOptVec_alt();

void sj3_alt() {
  for (int value : getVec_alt()) { // OK
  }

  for (int value : *getOptVec_alt()) { // expected-warning {{dereferencing a dangling pointer}}
    // expected-note@-1 {{temporary was destroyed at the end of the full expression}}
  }
}

std::string operator+(std::string_view sv1, std::string_view sv2) {
  return std::string(sv1) + std::string(sv2);
}

template <typename T>
T concat(const T &x, const T &y) {
  // TODO: Elide the deref for references?
  return x + y; // expected-warning {{returning a dangling Pointer}}
  // expected-note@-1 {{temporary was destroyed at the end of the full expression}}
}

void sj4() {
  std::string_view s = "foo"_s;
  // expected-note@-1 {{temporary was destroyed at the end of the full expression}}
  use(s); // expected-warning {{passing a dangling pointer as argument}}

  std::string_view hi = "hi";
  auto xy = concat(hi, hi);
  // expected-note@-1 {{in instantiation of function template specialization 'P0936::concat<std::string_view>' requested here}}
}

std::string GetString();

void f(std::string_view);

const std::string &findWithDefault(const std::map<int, std::string> &m, int key, const std::string &default_value) {
  auto iter = m.find(key);
  if (iter != m.end())
    return iter->second;
  else
    return default_value;
}

void sj5() {
  use(GetString()); // OK

  std::string_view sv = GetString();
  // expected-note@-1 {{temporary was destroyed at the end of the full expression}}
  use(sv); // expected-warning {{passing a dangling pointer as argument}}

  std::map<int, std::string> myMap;
  const std::string &val = findWithDefault(myMap, 1, "default value");
  // expected-note@-1 {{temporary was destroyed at the end of the full expression}}

  // TODO: diagnostic should not say "dereferencing"
  use(val); // expected-warning {{dereferencing a dangling pointer}}
}

} // namespace P0936
