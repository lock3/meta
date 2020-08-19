// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "reflection_query.h"

namespace std {

struct true_type {
  static bool value;
};

bool true_type::value = true;

struct false_type {
  static bool value;
};

bool false_type::value = false;

template <class A, class B> struct is_same : public false_type {};
template <class A> struct is_same<A, A> : public true_type {};

template <class A, class B>
bool is_same_v(A, B) {
  return is_same<A, B>::value;
}

} // end namespace std

namespace meta {

using info = decltype(reflexpr(void));

consteval info get_type(info x) {
  return __reflect(query_get_type, x);
}

} // end namespace meta

template<typename T>
void foo() {
  constexpr auto type = reflexpr(T);
  static_assert(std::is_same_v<typename(type), T>);

  typename(type) x = 0;
  static_assert(std::is_same_v<typename(type), int>);
}

struct S { };
struct S2;

constexpr int test() {
  int local = 0;

  {
    constexpr meta::info x = reflexpr(local);
    constexpr meta::info t = meta::get_type(x);

    static_assert(t == reflexpr(int));
    static_assert(std::is_same_v<typename(t), int>);
  }

  {
    S s;
    constexpr meta::info x = reflexpr(s);
    constexpr meta::info t = meta::get_type(x);

    static_assert(t == reflexpr(S));
    static_assert(std::is_same_v<typename(t), S>);
  }

  return 0;
}

constexpr meta::info get_type() {
  return reflexpr(S);
}

template<typename T, meta::info X = reflexpr(T)>
constexpr T check() {
  typename(X) var = 0;
  return var + 42;
}

typename(get_type()) global;

int main(int argc, const char* argv[]) {
  constexpr int n = test();

  static_assert(check<int>() == 42);
  static_assert(check<const int>() == 42);
}
