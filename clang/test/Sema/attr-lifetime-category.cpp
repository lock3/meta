// RUN: %clang -std=c++17 -fsyntax-only -Wlifetime -Wlifetime-debug -Xclang -verify %s

#include <any>
#include <memory>
#include <optional>
#include <queue>
#include <regex>
#include <stack>
#include <variant>
#include <vector>
//#include <span> I need newer C++ lib
#include <string_view>

template <typename T>
void __lifetime_type_category(){};

class [[gsl::Owner]] my_owner {
  int i;
};
template <class T>
class [[gsl::Pointer]] my_pointer {
  int i;
};

void owner() {
  // use decltype to force template instantiation
  __lifetime_type_category<my_owner>();                              // expected-warning {{Owner}}
  __lifetime_type_category<decltype(std::vector<int>())>();          // expected-warning {{Owner}}
  __lifetime_type_category<decltype(std::unique_ptr<int>())>();      // expected-warning {{Owner}}
  __lifetime_type_category<decltype(std::stack<int>())>();           // expected-warning {{Owner}}
  __lifetime_type_category<decltype(std::queue<int>())>();           // expected-warning {{Owner}}
  __lifetime_type_category<decltype(std::priority_queue<int>())>();  // expected-warning {{Owner}}
  __lifetime_type_category<decltype(std::optional<int>())>();        // expected-warning {{Owner}}
  __lifetime_type_category<decltype(std::variant<int, char *>())>(); // expected-warning {{Owner}}
  __lifetime_type_category<decltype(std::any())>();                  // expected-warning {{Owner}}
}

void pointer() {
  __lifetime_type_category<decltype(my_pointer<int>())>();                   // expected-warning {{Pointer}}
  __lifetime_type_category<decltype(std::regex_iterator<const char *>())>(); // expected-warning {{Pointer}}
  __lifetime_type_category<decltype(std::basic_string_view<char>())>();      // expected-warning {{Pointer}}
  //__lifetime_type_category<std::span<int>>(); // I need newer C++ lib

  int i;
  auto L = [&i]() { return i; };
  __lifetime_type_category<decltype(L)>(); // expected-warning {{Pointer}}

  __lifetime_type_category<int *>(); // expected-warning {{Pointer}}
  __lifetime_type_category<int &>(); // expected-warning {{Pointer}}
  __lifetime_type_category<decltype(std::regex())>();                   // expected-warning {{Pointer}}
  __lifetime_type_category<decltype(std::reference_wrapper<int>(i))>(); // expected-warning {{Pointer}}
  // TODO: not detected because the type name is std::_Bit_reference
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
  // TODO does the paper intend this to be an Aggregate? CXXRecorDecl::isAggregate returns false
  __lifetime_type_category<C>(); // expected-warning {{Aggregate}}
}

void value() {
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
}
