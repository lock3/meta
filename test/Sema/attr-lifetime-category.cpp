// RUN: %clang_cc1 -std=c++1z -fsyntax-only -Wlifetime -Wlifetime-debug -verify %s

// TODO: regression tests should not include the standard libary,
//       but we should have a way to test against real implementations.
namespace std {
struct any {
  any();
  any(const any &);
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
  variant(const variant &);
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
  T2 &operator*();
  ~shared_ptr();
};
} // namespace std

template <typename T>
void __lifetime_type_category() {}

template <typename T>
void __lifetime_type_category_arg(T arg) {}

class [[gsl::Owner]] my_owner {
  int &operator*();
  int i;
};
template <class T>
class [[gsl::Pointer]] my_pointer {
  T &operator*();
  int i;
};

class [[gsl::Owner]] owner_failed_deduce {
  // TODOexpected-warning@-1 {{cannot deduce deref type, ignoring attribute}}
  int i;
};

template <int I, typename T>
class [[gsl::Owner]] template_owner_failed_deduce {
  // TODOexpected-warning@-1 {{cannot deduce deref type, ignoring attribute}}
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
  __lifetime_type_category<my_owner>();                             // expected-warning {{Owner}}
  __lifetime_type_category<decltype(std::vector<int>())>();         // expected-warning {{Owner}}
  __lifetime_type_category<decltype(std::unique_ptr<int>())>();     // expected-warning {{Owner}}
  __lifetime_type_category<decltype(std::shared_ptr<int>())>();     // expected-warning {{Owner}}
  __lifetime_type_category<decltype(std::stack<int>())>();          // expected-warning {{Owner}}
  __lifetime_type_category<decltype(std::queue<int>())>();          // expected-warning {{Owner}}
  __lifetime_type_category<decltype(std::priority_queue<int>())>(); // expected-warning {{Owner}}
  __lifetime_type_category<decltype(std::optional<int>())>();       // expected-warning {{Owner}}
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

  __lifetime_type_category<decltype(owner_failed_deduce())>();                   // expected-warning {{Value}}
  __lifetime_type_category<decltype(template_owner_failed_deduce<3, void>())>(); // expected-warning {{Value}}
}

namespace classTemplateInstantiation {
// First template parameter is a non-type to avoid falling back
// to deducing DerefType from first template parameter.
template <int I, typename T>
struct iterator {
  T &operator*();
};

template <int I, typename T>
struct vector {
  iterator<I, T> begin();
  iterator<I, T> end();
  ~vector();
};

void f() {
  // Clang would create an ClassTemplateSpecializationDecl for
  // iterator<0, int>, but create no definition, i.e. we could not see
  // iterator<0, int>::operator*(), unless that operator is explicitly called
  // by the program. The lifetime checker now forces and definition of
  // iterator<0, int> to be able to deduce the DerefType(iterator<0, int>).
  __lifetime_type_category<decltype(vector<0, int>())>(); // expected-warning {{Owner with pointee int}}
}
} // namespace classTemplateInstantiation

namespace functionTemplateInstantiation {
// First template parameter is a non-type to avoid falling back
// to deducing DerefType from first template parameter.
template <int I, typename T>
struct pointer {
  template<typename D = T>
  D& operator*();
};

void f() {
  // Clang creates a FunctionTemplateDecl for operator*()
  // but not specialisation for the default case (D = T)
  // unless begin() is used in the program. The lifetime checker now forces
  // and specialisation of operator*() to be able to deduce the DerefType(pointer<0, int>).
  __lifetime_type_category<decltype(pointer<0, int>())>(); // expected-warning {{Pointer with pointee int}}
}
} // namespace functionTemplateInstantiation

namespace defaultedDestructor {
  struct P {
    ~P() = default; // still trivial destructor
    int* operator->();
  };
  void f() {
    __lifetime_type_category<P>(); // expected-warning {{Pointer with pointee int}}
  }
}