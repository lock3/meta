// RUN: %clang_cc1 -freflection -std=c++1z %s

class test_class {
  consteval -> __fragment class {
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

