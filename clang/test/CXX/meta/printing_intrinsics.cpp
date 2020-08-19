// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "reflection_query.h"

namespace meta {
  using info = decltype(reflexpr(void));
}

constexpr meta::info invalid_refl = __invalid_reflection("custom error message");

template<typename T>
struct S1 {
  constexpr S1() = default;
  constexpr T foo() { return T(); }
  T variable;
};

void test() {
  constexpr auto x1 = reflexpr(int);

  constexpr int* y1 = nullptr;
  constexpr auto x2 = reflexpr(y1);

  constexpr int y2 = int();
  constexpr auto x3 = reflexpr(y2);

  constexpr const int y3 = int();
  constexpr auto x4 = reflexpr(y3);

  constexpr S1<int> y4 = S1<int>();
  constexpr auto x5 = reflexpr(y4);

  constexpr auto x6 = reflexpr(S1<int>::foo);
  constexpr auto x7 = reflexpr(S1<int>::variable);

  // Generate output from reflections
  constexpr auto x1_pretty_print = __reflect_pretty_print(x1);
  constexpr auto x1_dump = __reflect_dump(x1);
  constexpr auto x2_pretty_print = __reflect_pretty_print(x2);
  constexpr auto x2_dump = __reflect_dump(x2);
  constexpr auto x3_pretty_print = __reflect_pretty_print(x3);
  constexpr auto x3_dump = __reflect_dump(x3);
  constexpr auto x4_pretty_print = __reflect_pretty_print(x4);
  constexpr auto x4_dump = __reflect_dump(x4);
  constexpr auto x5_pretty_print = __reflect_pretty_print(x5);
  constexpr auto x5_dump = __reflect_dump(x5);
  constexpr auto x6_pretty_print = __reflect_pretty_print(x6);
  constexpr auto x6_dump = __reflect_dump(x6);
  constexpr auto x7_pretty_print = __reflect_pretty_print(x7);
  constexpr auto x7_dump = __reflect_dump(x7);

  // Generate output from literals
  constexpr int world_num = 11;
  // constexpr char *user_name = "Bob";

  constexpr auto l1_print = __reflect_print("hello ", " world ", 1);
  constexpr auto l2_print = __reflect_print("hello ", " world ", world_num);
  // constexpr auto l3_print = __reflect_print(user_name, " is not a valid user.");

  constexpr auto __dummy_pretty_print = __reflect_pretty_print(invalid_refl);
  constexpr auto __dummy_dump = __reflect_dump(invalid_refl);
}

class C {
  int y = 0;

  template<typename T>
  struct Template {};

  template<>
  struct Template<int> {};
};

enum E {
  E_A, E_B
};

enum class EC {
  A, B
};

union U {
  int a;
  float b;
};

void test_types() {
  {
    constexpr auto type = reflexpr(C);
    constexpr auto definition = __reflect(query_get_definition, type);
    { constexpr auto __dummy = __reflect_pretty_print(type); }
    { constexpr auto __dummy = __reflect_pretty_print(definition); }
  }
  {
    constexpr auto type = reflexpr(E);
    constexpr auto definition = __reflect(query_get_definition, type);
    { constexpr auto __dummy = __reflect_pretty_print(type); }
    { constexpr auto __dummy = __reflect_pretty_print(definition); }
  }
  {
    constexpr auto type = reflexpr(EC);
    constexpr auto definition = __reflect(query_get_definition, type);
    { constexpr auto __dummy = __reflect_pretty_print(type); }
    { constexpr auto __dummy = __reflect_pretty_print(definition); }
  }
  {
    constexpr auto type = reflexpr(U);
    constexpr auto definition = __reflect(query_get_definition, type);
    { constexpr auto __dummy = __reflect_pretty_print(type); }
    { constexpr auto __dummy = __reflect_pretty_print(definition); }
  }
}

template<typename T, int WN>
void test_dependent() {
  constexpr auto x1 = reflexpr(T);

  constexpr T* y1 = nullptr;
  constexpr auto x2 = reflexpr(y1);

  constexpr T y2 = T();
  constexpr auto x3 = reflexpr(y2);

  constexpr const T y3 = T();
  constexpr auto x4 = reflexpr(y3);

  constexpr S1<T> y4 = S1<T>();
  constexpr auto x5 = reflexpr(y4);

  constexpr auto x6 = reflexpr(S1<T>::foo);
  constexpr auto x7 = reflexpr(S1<T>::variable);

  // Generate output
  constexpr auto x1_pretty_print = __reflect_pretty_print(x1);
  constexpr auto x1_dump = __reflect_dump(x1);
  constexpr auto x2_pretty_print = __reflect_pretty_print(x2);
  constexpr auto x2_dump = __reflect_dump(x2);
  constexpr auto x3_pretty_print = __reflect_pretty_print(x3);
  constexpr auto x3_dump = __reflect_dump(x3);
  constexpr auto x4_pretty_print = __reflect_pretty_print(x4);
  constexpr auto x4_dump = __reflect_dump(x4);
  constexpr auto x5_pretty_print = __reflect_pretty_print(x5);
  constexpr auto x5_dump = __reflect_dump(x5);
  constexpr auto x6_pretty_print = __reflect_pretty_print(x6);
  constexpr auto x6_dump = __reflect_dump(x6);
  constexpr auto x7_pretty_print = __reflect_pretty_print(x7);
  constexpr auto x7_dump = __reflect_dump(x7);

  // Generate output from reflections
  constexpr int world_num = WN;

  constexpr auto l1_print = __reflect_print("hello ", " world ", 3);
  constexpr auto l2_print = __reflect_print("hello ", " world ", world_num);
}

struct S { };

void test_templates() {
  test_dependent<int, 9>();
  test_dependent<S, 11>();
}

int main() {
  test();
  test_types();
  test_templates();
  return 0;
}
