// RUN: %clang_cc1 -std=c++1z -freflection %s

namespace N1 { 
  namespace N2 {
    struct S { };
  }
  int x;
}

constexpr! int nesting(meta::info d) {
  int n = 0;
  meta::info p = parent(d);
  while (!is_translation_unit(p)) {
    p = parent(p);
    ++n;
  }
  return n;
}

int main(int argc, char* argv[]) {

  constexpr meta::info n1 = reflexpr(N1);
  constexpr meta::info n2 = reflexpr(N1::N2);
  constexpr meta::info s = reflexpr(N1::N2::S);
  constexpr meta::info x = reflexpr(N1::x);

  static_assert(parent(n2) == n1);
  static_assert(parent(s) == n2);
  static_assert(parent(x) == n1);

  constexpr meta::info t1 = parent(n1);
  static_assert(is_translation_unit(t1));

  constexpr meta::info null = parent(t1);
  static_assert(is_null(null));

  static_assert(nesting(s) == 2);
}
