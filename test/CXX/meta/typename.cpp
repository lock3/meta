// RUN: %clang_cc1 -std=c++1z -freflection %s

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
    constexpr meta::info t = type(x);
    // meta::compiler.print(t);
    (void)__reflect_print(t);
    assert(t == reflexpr(int));
    static_assert(std::is_same_v<typename(t), int>);
  }

  {
    S s;
    constexpr meta::info x = reflexpr(s);
    constexpr meta::info t = type(x);
    // meta::compiler.print(t);
    (void)__reflect_print(t);
    assert(t == reflexpr(S));
    static_assert(std::is_same_v<typename(t), S>);
  }

  return 0;
}

constexpr meta::info get_type()
{
  return reflexpr(S);
}

template<typename T, meta::info X = reflexpr(T)>
constexpr T check()
{
  // constexpr int dummy = (meta::compiler.print(X), 0);
  typename(X) var = 0;
  return var + 42;
}

typename(get_type()) global;

int main(int argc, const char* argv[]) {
  constexpr int n = test();
  // constexpr int k = (meta::compiler.print(reflexpr(global)), 0);

  static_assert(check<int>() == 42);
  static_assert(check<const int>() == 42);
}
