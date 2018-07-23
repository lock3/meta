#include <experimental/meta>

using namespace std::experimental;

int x1;
static int x2;

void f1() { }
void f2();
static inline void f3() { }
constexpr int f4() { return 0; }
void f5(int) = delete;

class C1 { };

class C2;

union U1 { };

enum E1 { X };
enum class E2 { X };
enum class E3 : int;


struct S1 {
  int x1;
  mutable int x2;
  static int x3;
  static constexpr int x4 = 0;
  int x5 : 4;

  void f1() { }

  class C { };
};

namespace N { 
  inline namespace M { }
}

int main(int argc, char* argv[]) {

  // int x1 -- global
  {
    constexpr meta::info r = reflexpr(x1);
    static_assert(kind(r) == meta::variable_decl);
  }

  // static int x2 -- global
  {
    constexpr meta::info r = reflexpr(x2);
    static_assert(kind(r) == meta::variable_decl);
  }

  // Functions
  
  // void f1() { }
  {
    constexpr meta::info r = reflexpr(f1);
    static_assert(kind(r) == meta::function_decl);
  }

  // void f2();
  {
    constexpr meta::info r = reflexpr(f2);
    static_assert(kind(r) == meta::function_decl);
  }

  // static inline void f3() { }
  {
    constexpr meta::info r = reflexpr(f3);
    static_assert(kind(r) == meta::function_decl);
  }

  // constexpr int f4() { return 0; }
  {
    constexpr meta::info r = reflexpr(f4);
    static_assert(kind(r) == meta::function_decl);
  }

  // void f5(int) = delete;
  {
    constexpr meta::info r = reflexpr(f5);
    static_assert(kind(r) == meta::function_decl);
  }

  // Namespaces
  
  // namespace N
  {
    constexpr meta::info r = reflexpr(N);
    static_assert(kind(r) == meta::namespace_decl);
  }

  // inline namespace N::M
  {
    constexpr meta::info r = reflexpr(N::M);
    static_assert(kind(r) == meta::namespace_decl);
  }

  // Classes

  // class C1 { };
  {
    constexpr meta::info r = reflexpr(C1);
    static_assert(kind(r) == meta::class_decl);
  }

  // class C2;
  {
    constexpr meta::info r = reflexpr(C2);
    static_assert(kind(r) == meta::class_decl);
  }

  // struct S1 { ... };
  {
    constexpr meta::info r = reflexpr(S1);
    static_assert(kind(r) == meta::class_decl);
  }

  // class S1::C { ... };
  {
    constexpr meta::info r = reflexpr(S1::C);
    static_assert(kind(r) == meta::class_decl);
  }
  
  // union U1 { ... };
  {
    constexpr meta::info r = reflexpr(U1);
    static_assert(kind(r) == meta::class_decl);
  }

  // Data members

  // int S1::x1;
  {
    constexpr meta::info r = reflexpr(S1::x1);
    static_assert(kind(r) == meta::data_member_decl);
  }

  // mutable int S1::x2;
  {
    constexpr meta::info r = reflexpr(S1::x2);
    static_assert(kind(r) == meta::data_member_decl);
  }

  // static int S1::x3;
  {
    constexpr meta::info r = reflexpr(S1::x3);
    static_assert(kind(r) == meta::data_member_decl);
  }

  // static constexpr int S1::x4;
  {
    constexpr meta::info r = reflexpr(S1::x4);
    static_assert(kind(r) == meta::data_member_decl);
  }

  // int S1::x5 : 4;
  {
    constexpr meta::info r = reflexpr(S1::x5);
    static_assert(kind(r) == meta::data_member_decl);
  }

  // Member functions

  // int S1::f1() { }
  {
    constexpr meta::info r = reflexpr(S1::f1);
    static_assert(kind(r) == meta::member_function_decl);
  }

  // FIXME: Add more tests for member functions.

  // Enums

  // enum E1 { ... }
  {
    constexpr meta::info r = reflexpr(E1);
    static_assert(kind(r) == meta::enum_decl);
  }

  // enum class E2 { ... }
  {
    constexpr meta::info r = reflexpr(E2);
    static_assert(kind(r) == meta::enum_decl);
  }

  // enum class : int;
  {
    constexpr meta::info r = reflexpr(E3);
    static_assert(kind(r) == meta::enum_decl);
  }

  // Enumerators

  // enum E1 { X }
  {
    constexpr meta::info r = reflexpr(X);
    static_assert(kind(r) == meta::enumerator_decl);
  }

  // enum class E2 { X }
  {
    constexpr meta::info r = reflexpr(E2::X);
    static_assert(kind(r) == meta::enumerator_decl);
  }

}
