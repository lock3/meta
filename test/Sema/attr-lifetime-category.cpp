// RUN: %clang_cc1 -std=c++1z -fsyntax-only -Wlifetime -Wlifetime-debug -verify %s

// TODO: regression tests should not include the standard libary,
//       but we should have a way to test against real implementations.
namespace std {
struct any {
  any();
  any(const any&);
  ~any();
};

template <typename T>
struct basic_regex {};

template <typename T>
struct regex_iterator {
  T &operator*() const;
};

using regex = basic_regex<char>;

template <typename T>
struct unique_ptr {
  T &operator*() const;
  ~unique_ptr();
};

template <typename T>
struct optional {};

template <typename T>
struct vector {
  T *begin();
  T *end();
  ~vector();
};

struct _Bit_reference {
  void flip();
};

template <>
struct vector<bool> {
  using reference = _Bit_reference;
  bool *begin();
  bool *end();
  ~vector();
};

template <typename T>
struct span {
  T *begin();
  T *end();
};

template <typename T>
struct basic_string_view {
  T *begin();
  T *end();
};

using string_view = basic_string_view<char>;

template <typename T>
struct stack {};

template <typename T>
struct queue {};

template <typename T>
struct priority_queue {};

template <typename... T>
struct variant {
  variant();
  variant(const variant&);
  ~variant();
};

template <typename T>
struct reference_wrapper {
  template <typename U>
  reference_wrapper(U &&);
  operator T &() const;
};

template <typename T>
struct shared_ptr {
  template <typename T2 = T>
  T2& operator*();
  ~shared_ptr();
};
} // namespace std

template <typename T>
void __lifetime_type_category() {}

template <typename T>
void __lifetime_type_category_arg(T arg) {}

class [[gsl::Owner]] my_owner {
  int i;
};
template <class T>
class [[gsl::Pointer]] my_pointer {
  int i;
};

struct my_implicit_owner {
  int operator*();
  int *begin() const;
  int *end() const;
  ~my_implicit_owner();
};

struct my_derived_owner : my_implicit_owner {
};

void owner() {
  // Use decltype to force template instantiation.
  __lifetime_type_category<my_owner>();                              // expected-warning {{Owner}}
  __lifetime_type_category<decltype(std::vector<int>())>();          // expected-warning {{Owner}}
  __lifetime_type_category<decltype(std::unique_ptr<int>())>();      // expected-warning {{Owner}}
  __lifetime_type_category<decltype(std::shared_ptr<int>())>();      // expected-warning {{Owner}}
  __lifetime_type_category<decltype(std::stack<int>())>();           // expected-warning {{Owner}}
  __lifetime_type_category<decltype(std::queue<int>())>();           // expected-warning {{Owner}}
  __lifetime_type_category<decltype(std::priority_queue<int>())>();  // expected-warning {{Owner}}
  __lifetime_type_category<decltype(std::optional<int>())>();        // expected-warning {{Owner}}
  using IntVector = std::vector<int>;
  __lifetime_type_category<decltype(IntVector())>();         // expected-warning {{Owner}}
  __lifetime_type_category<decltype(my_implicit_owner())>(); // expected-warning {{Owner}}
  __lifetime_type_category<decltype(my_derived_owner())>();  // expected-warning {{Owner}}
}

void pointer() {
  __lifetime_type_category<decltype(my_pointer<int>())>();                   // expected-warning {{Pointer}}
  __lifetime_type_category<decltype(std::regex_iterator<const char *>())>(); // expected-warning {{Pointer}}
  __lifetime_type_category<decltype(std::basic_string_view<char>())>();      // expected-warning {{Pointer}}
  __lifetime_type_category<decltype(std::string_view())>();                  // expected-warning {{Pointer}}
  __lifetime_type_category<decltype(std::span<int>())>();                    // expected-warning {{Pointer}}

  int i;
  __lifetime_type_category<int *>();                                    // expected-warning {{Pointer}}
  __lifetime_type_category<int &>();                                    // expected-warning {{Pointer}}
  __lifetime_type_category<decltype(std::regex())>();                   // expected-warning {{Pointer}}
  __lifetime_type_category<decltype(std::reference_wrapper<int>(i))>(); // expected-warning {{Pointer}}
  __lifetime_type_category_arg(std::vector<bool>::reference());         // expected-warning {{Pointer}}
  __lifetime_type_category<decltype(std::vector<bool>::reference())>(); // expected-warning {{Pointer}}
}

void aggregate() {
  struct S {
    int i;
  };
  __lifetime_type_category<S>(); // expected-warning {{Aggregate}}

  class C {
    C() { i = 1; }

  public:
    int i;
  };
  // does the paper intend this to be an Aggregate? CXXRecorDecl::isAggregate returns false
  //__lifetime_type_category<C>(); // TODOexpected-warning {{Aggregate}}
}

void value() {
  __lifetime_type_category<decltype(std::variant<int, char *>())>(); // expected-warning {{Value}}
  __lifetime_type_category<decltype(std::any())>();                  // expected-warning {{Value}}

  // no public data members
  class C1 {
    C1() { i = 1; }
    int i;
  };
  __lifetime_type_category<C1>(); // expected-warning {{Value}}

  // user provided move/copy operations
  class C2 {
    C2(const C2 &);

  public:
    int i;
  };
  __lifetime_type_category<C2>(); // expected-warning {{Value}}

  int i = 0;
  auto L = [i]() { return i; };
  __lifetime_type_category<decltype(L)>(); // expected-warning {{Value}}
  auto L2 = [&i]() { return i; };
  __lifetime_type_category<decltype(L2)>(); // expected-warning {{Value}}

  class C3;
  __lifetime_type_category<C3>(); // expected-warning {{Value}}
}
