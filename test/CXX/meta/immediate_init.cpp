// RUN: %clang_cc1 -emit-llvm -std=c++2a -freflection -o %t %s

struct Foo {
  template<typename T>
  consteval Foo(T var) {
  }
};

int main() {
  Foo C = Foo(reflexpr(Foo));
  return 0;
}
