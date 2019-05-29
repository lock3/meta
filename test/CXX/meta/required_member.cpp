// RUN: %clang_cc1 -fsyntax-only -freflection -std=c++2a -verify %s
// expected-no-diagnostics

#define assert(E) if (!(E)) __builtin_abort();

struct Fields {
  int x = int();

  consteval -> __fragment struct {
    requires int x;
    void foo() {
      ++x;
    }
  };
};

struct Methods {
  int f() { return int(); }
  int f(int a) { return a; }

  consteval -> __fragment struct {
    requires int f(int a);
    requires int f();
    int foo() {
      return f(30);
    }
  };
};

template<typename T>
struct DependentFields {
  int x = T();

  consteval -> __fragment struct {
    requires T x;
    void foo() {
      ++x;
    }
  };
};

template<typename T>
struct DependentMethods {
  T f() { return int(); }
  T f(int a) { return a; }

  consteval -> __fragment struct {
    requires T f(int a);
    requires T f();
    T foo() {
      return f(30);
    }
  };
};


int main() {
  Fields req_field;
  req_field.foo();
  assert(req_field.x == 1);

  Methods req_method;
  assert(req_method.foo() == 30);

  DependentFields<int> dep_field;
  dep_field.foo();
  assert(dep_field.x == 1);

  DependentMethods<int> dep_method;
  assert(dep_method.foo() == 30);
}
