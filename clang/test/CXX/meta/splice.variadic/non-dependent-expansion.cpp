// RUN: %clang_cc1 -std=c++2a -freflection %s

using info = decltype(^void);

template<typename ...T> constexpr auto dc_add() { return (... + T(1)); }
template<info ...T> constexpr auto dc_refl_add() { return (... + typename [:T:](1)); }

struct tri_construct {
  int total;

  constexpr tri_construct(int x, int y, int z) : total(x + y + z) { }
};

template<typename ...T> constexpr auto tri_size() { return tri_construct(T(1)...).total; }
template<info ...T> constexpr auto tri_refl_ty_size() { return tri_construct(typename [:T:](1)...).total; }
template<info ...T> constexpr auto tri_refl_expr_size() { return tri_construct([:T:]...).total; }

template<int ...T> constexpr auto tri_refl_expr_size() { return tri_construct(T...).total; }

int main() {
  static_assert(dc_add<int, int, int>() == 3);
  static_assert(dc_refl_add<^int, ^int, ^int>() == 3);

  static_assert(tri_size<int, int, int>() == 3);
  static_assert(tri_refl_ty_size<^int, ^int, ^int>() == 3);
  static_assert(tri_refl_expr_size<^1, ^1, ^1>() == 3);
  return 0;
}
