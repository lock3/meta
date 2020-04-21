// RUN: %clang_cc1 -freflection -std=c++2a %s

class test_class {
  consteval -> fragment class {
    class Sub {
      void f() {
        auto var = 0;
        ++var;
      }
    };
  };
};

int main() {
  return 0;
}
