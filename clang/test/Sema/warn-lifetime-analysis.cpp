// RUN: %clang_cc1 -fcxx-exceptions -fsyntax-only -verify -Wlifetime -Wno-dangling %s
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
  size_t length() const noexcept;
  const char *c_str() const noexcept;
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
  const T &operator[](size_t) const;
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

namespace gsl {
template <typename T>
using nullable = T;


template <typename T>
using not_null = T;
} // namespace gsl

struct Owner {
  ~Owner();
  int m;
  int f();
};

struct [[gsl::Pointer]] my_pointer {
  my_pointer &operator=(int *);
  int &operator*();
  operator bool() const;
};

void deref_uninitialized() {
  int *p;        // expected-note {{it was never initialized here}}
  *p = 3;        // expected-warning {{dereferencing a dangling pointer}}
  my_pointer p2; // expected-note {{default-constructed pointers are assumed to be null}}
  *p2;           // expected-warning {{passing a null pointer as argument where a non-null pointer is expected}}
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

int global;
void optional_output_argument(my_pointer *out) {
  if (out)
    *out = &global;
}

void optional_output_argument2(my_pointer *out) {
  if (!out)
    return;
  *out = &global;
}

void do_not_validate_output_on_exceptions(bool b, my_pointer &out) {
  if (b)
    throw 5;
  out = &global;
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

void delete_pointee(int *p) {
  int *q = p;
  delete q; // expected-note {{deleted here}}
  (void)*p; // expected-warning {{dereferencing a dangling pointer}}
}

void delete_pointee_userdefined(my_pointer p) {
  if (!p)
    return;
  delete &(*p); // expected-note {{deleted here}}
  (void)*p;     // expected-warning {{passing a dangling pointer as argument}}
}

void copy_null_ptr(int *p, my_pointer p2) {
  gsl::not_null<int* > q = p;        // expected-warning {{assigning a possibly null pointer to a non-null object}}
  q = p;                             // expected-warning {{assigning a possibly null pointer to a non-null object}}
  gsl::not_null<my_pointer> q2 = p2; // expected-warning {{assigning a possibly null pointer to a non-null object}}
  q2 = p2;                           // expected-warning {{assigning a possibly null pointer to a non-null object}}
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
  return q; // expected-warning {{returning a dangling pointer}}
  // expected-note@-1 {{pointee 'i' left the scope here}}
}

const int &return_wrong_ptr2(std::vector<int> &v,
                             const std::vector<int> &v2) {
  return v2[0]; // expected-warning {{returning a pointer with points-to set (**v2) where points-to set (**v) is expected}}
}

gsl::not_null<int *> return_null_ptr() {
  return nullptr; // expected-warning {{returning a null pointer where a non-null pointer is expected}}
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
  // suppressed further diagnostics here:
  (void)*q;
}

void f(const char **values) {
  // Array subscription into poiter 'values' is not allowed and disables
  // the analysis. In particular, we should not see
  //     warning: returning a dangling pointer as output value '*values'.
  values[0] = "hello";
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

const char * hello2() {
  std::string s;
  return s.c_str(); // expected-warning {{dangling}} expected-note {{pointee 's' left}}
}

bool uninitialized_output_param(int **p) { // expected-note {{it was never initialized here}}
  return true; // expected-warning {{returning a dangling pointer as output value '*p'}}
}

std::initializer_list<int> dangling_initializer_list() {
  return {1, 2, 3}; // expected-warning {{returning a dangling pointer}}
  // expected-note@-1 {{temporary was destroyed at the end of the full expression}}
}

int* static_or_null() {
  return nullptr; // OK
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
  return x + y; // expected-warning {{returning a dangling pointer}}
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

// https://github.com/mgehre/llvm-project/issues/62
namespace bug_report_63 {

int printf(const char *format, ...);

const char *longer(std::string const &s1, std::string const &s2) {
  if (s1.length() > s2.length()) {
    return s1.c_str();
  }
  return s2.c_str();
}

const char *f1();
const char *f2();

int main() {
  const char *lg = longer(f1(), f2()); // expected-note {{temporary was destroyed at the end of the full expression}}
  printf("longer arg is %s", lg);      // expected-warning {{passing a dangling pointer as argument}}
  return 0;
}
} // namespace bug_report_63

namespace bug_report_66 {

class [[gsl::Owner]] basic_string {
public:
  void a();
};
void b(basic_string &c) {
  c.a();
  c; // expected-warning {{expression result unused}}
}
} // namespace bug_report_66

namespace varargs {
void f(int, ...);

void g() {
  int *p;
  f(1, &p);
  *p = 5; // no-warning
}

} // namespace varargs
