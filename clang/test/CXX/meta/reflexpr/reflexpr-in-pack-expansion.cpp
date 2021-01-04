// RUN: %clang_cc1 -std=c++2a -freflection %s

using info = decltype(reflexpr(void));

template<int I, int ...Is>
consteval auto collect_exprs() {
  info results [] = { reflexpr(Is)... };
  return results[I];
}

template<int I, typename ...Ts>
consteval auto collect_types() {
  info results [] = { reflexpr(Ts)... };
  return results[I];
}

int main() {
  static_assert([< collect_exprs<1, 1, 2, 3>() >] == 2);
  static_assert(collect_types<1, int, float, double>() == reflexpr(float));

  return 0;
}
