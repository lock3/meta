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
  }

  // static int x2 -- global
  {
    constexpr meta::info r = reflexpr(x2);
  }

  // Functions
  
  // void f1() { }
  {
    constexpr meta::info r = reflexpr(f1);
  }

  // void f2();
  {
    constexpr meta::info r = reflexpr(f2);
  }

  // static inline void f3() { }
  {
    constexpr meta::info r = reflexpr(f3);
  }

  // constexpr int f4() { return 0; }
  {
    constexpr meta::info r = reflexpr(f4);
  }

  // void f5(int) = delete;
  {
    constexpr meta::info r = reflexpr(f5);
  }

  // Namespaces
  
  // namespace N
  {
    constexpr meta::info r = reflexpr(N);
  }

  // inline namespace N::M
  {
    constexpr meta::info r = reflexpr(N::M);
  }

  // Classes

  // class C1 { };
  {
    constexpr meta::info r = reflexpr(C1);
  }

  // class C2;
  {
    constexpr meta::info r = reflexpr(C2);
  }

  // struct S1 { ... };
  {
    constexpr meta::info r = reflexpr(S1);
  }

  // class S1::C { ... };
  {
    constexpr meta::info r = reflexpr(S1::C);
  }
  
  // union U1 { ... };
  {
    constexpr meta::info r = reflexpr(U1);
  }

  // Data members

  // int S1::x1;
  {
    constexpr meta::info r = reflexpr(S1::x1);
  }

  // mutable int S1::x2;
  {
    constexpr meta::info r = reflexpr(S1::x2);
  }

  // static int S1::x3;
  {
    constexpr meta::info r = reflexpr(S1::x3);
  }

  // static constexpr int S1::x4;
  {
    constexpr meta::info r = reflexpr(S1::x4);
  }

  // int S1::x5 : 4;
  {
    constexpr meta::info r = reflexpr(S1::x5);
  }

  // Member functions

  // int S1::f1() { }
  {
    constexpr meta::info r = reflexpr(S1::f1);
  }

  // FIXME: Add more tests for member functions.

  // Enums

  // enum E1 { ... }
  {
    constexpr meta::info r = reflexpr(E1);
  }

  // enum class E2 { ... }
  {
    constexpr meta::info r = reflexpr(E2);
  }

  // enum class : int;
  {
    constexpr meta::info r = reflexpr(E3);
  }

  // Enumerators

  // enum E1 { X }
  {
    constexpr meta::info r = reflexpr(X);
  }

  // enum class E2 { X }
  {
    constexpr meta::info r = reflexpr(E2::X);
  }

}
