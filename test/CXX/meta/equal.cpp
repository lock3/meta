// RUN: %clang_cc1 -I%S/usr/include -I%S/usr/local/include/c++/v1 -std=c++1z -freflection %s

#include <experimental/meta>

using namespace std::experimental;

extern "C" int printf(const char*, ...);


struct S {
  int a, b, c;
};


template<meta::info X, typename T>
bool compare(const T& a, const T& b) {
  if constexpr (!is_null(X)) {
    if constexpr (is_data_member(X)) {
      auto p = valueof(X);
      if (a.*p != b.*p)
        return false;
    }
    return compare<next(X)>(a, b);
  }
  return true;
}

template<typename T>
bool equal(const T& a, const T& b) {
  return compare<front(reflexpr(T))>(a, b);
}

int main() {
  S s1 { 0, 0, 0 };
  S s2 { 0, 0, 1 };
  assert(equal(s1, s1));
  assert(!equal(s1, s2));
}
