// RUN: %clang -freflection -Wno-deprecated-fragment -std=c++2a %s

struct test {
  consteval -> __fragment struct {
    template<class U>
    void f() { }
  };

  test() {
    consteval -> __fragment this {
      auto ptr = &typename(reflexpr(test))::template f<void>;
      (*this.*ptr)();
    };
  }
};

template <class T>
struct templ_test {
  consteval -> __fragment struct {
    template<class U>
    void f() { }
  };

  templ_test() {
    consteval -> __fragment this {
      auto ptr = &typename(reflexpr(templ_test))::template f<void>;
      (*this.*ptr)();
    };
  }
};

int main() {
  test {};
  templ_test<void> {};
  return 0;
}

