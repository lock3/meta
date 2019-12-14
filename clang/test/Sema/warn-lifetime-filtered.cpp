// RUN: %clang_cc1 -fcxx-exceptions -fsyntax-only -verify -Wlifetime -Wlifetime-filter -Wno-dangling %s

namespace std {

template <typename T>
struct unique_ptr {
  T &operator*() const;
  T *get();
  ~unique_ptr();
};

struct string {
  struct iterator {
    char &operator*();
    iterator &operator++();
    bool operator!=(const iterator &) const;
  };
  string();
  string(const char *s);
  char &operator[](int i);
  ~string();
  iterator begin();
  iterator end();
  const char *c_str() const noexcept;
};

} // namespace std

void trival() {
    int *p = nullptr; // expected-note {{assigned here}}
    *p = 5; // expected-warning {{dereferencing a null pointer}}
}

void trival_invalidation() {
    std::string s;
    const char *p = s.c_str();
    s = "hello"; // expected-note {{modified here}}
    char c = *p; // expected-warning {{dereferencing a dangling pointer}}
    (void)c;
}

void conditional_invalidation(bool flag) {
    std::string s;
    const char *p = s.c_str();
    if (flag)
        s = "hello";  // expected-note {{modified here}}
    char c = *p;      // expected-warning {{dereferencing a possibly dangling pointer}}
    (void)c;
}

void domination_param(int* p, bool flag) { // expected-note {{the parameter is assumed to be potentially null. Consider using gsl::not_null<>, a reference instead of a pointer or an assert() to explicitly remove null}}
    if (flag) {
        *p = 5;     // expected-warning {{dereferencing a possibly dangling pointer}
    }
}

void no_post_domination_or_domination(bool flag) {
    int i = 0;
    int *p = &i;
    if (!flag) {
        p = nullptr;
    }
    if (flag) {
        *p = 5;
    }
}

void no_post_domination_or_domination_invalidation(bool flag) {
    std::string s;
    const char *p = s.c_str();
    if (!flag)
        s = "hello";
    if (flag) {
        char c = *p;
        (void)c;
    }
}

void domination(bool flag) {
    int *p = nullptr; // expected-note {{assigned here}}
    if (flag) {
        *p = 5; // expected-warning {{dereferencing a null pointer}}
    }
}

void domination_but_overwritten(bool flag) {
    int *p = nullptr;
    int i;
    p = &i;
    if (flag) {
        *p = 5;
    }
}

void domination_invalidation(bool flag) {
    std::string s;
    const char *p = s.c_str();
    s = "hello"; // expected-note {{modified here}}
    if (flag) {
        char c = *p; // expected-warning {{dereferencing a dangling pointer}}
        (void)c;
    }
}
