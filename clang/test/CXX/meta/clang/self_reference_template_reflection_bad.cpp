// RUN: %clang_cc1 -freflection -std=c++2a -fsyntax-only -verify %s

struct test {
// expected-note@-1 {{candidate constructor (the implicit copy constructor) not viable: requires 1 argument, but 0 were provided}}
// expected-note@-2 {{candidate constructor (the implicit move constructor) not viable: requires 1 argument, but 0 were provided}}
  consteval -> fragment struct {
    template<class U>
    void f() { }
  };

  test() {
    consteval -> fragment this {
      auto ptr = typename(reflexpr(test))::template f<void>;
      // expected-error@-1 {{variable 'ptr' with type 'auto' has incompatible initializer of type '<overloaded function type>'}}
      // expected-error@-2 {{declaration of variable 'ptr' with deduced type 'auto' requires an initializer}}
    };
  }
};

template <class T>
struct templ_test {
  consteval -> fragment struct {
    template<class U>
    void f() { }
  };

  templ_test() {
    consteval -> fragment this {
      auto ptr = typename(reflexpr(templ_test))::template f<void>;
      // expected-error@-1 {{variable 'ptr' with type 'auto' has incompatible initializer of type '<overloaded function type>'}}
    };
  }
};

int main() {
  test {}; // expected-error {{no matching constructor for initialization of 'test'}}
  templ_test<void> {};
  // expected-note@-1 {{in instantiation of member function 'templ_test<void>::templ_test' requested here}}
  return 0;
}

