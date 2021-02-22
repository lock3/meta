// RUN: %clang_cc1 -std=c++2a -freflection -fsyntax-only -verify %s

namespace member_function {

struct foo {
  void a(int);
  void a(float);

  void b(int);

  template<typename T>
  void c(T) { }

  template<typename T, typename R>
  R d(T in) { return in; }

  template<auto F, typename A>
  void call(A arg) {
    this->[:F:](arg);
  }

  // template<auto F, typename A>
  // void delegate(A arg) {
  //   this->template [:F:]<A, A>(arg);
  // }
};

struct bar {
  void a(int);
  // expected-error@-1 {{no matching member function for call to 'a'}}
  // expected-note@-2 {{no known conversion from 'member_function::foo' to 'member_function::bar' for object argument}}
};

void a(int);
void a(float);

void b(int);

void test() {
  foo f;
  f.call<^foo::a, int>(10);
  f.call<^foo::b, int>(10);
  f.call<^foo::c, int>(10);
  // f.delegate<^foo::d, int>(10);

  f.call<^bar::a, int>(10); // expected-note {{in instantiation}}
  f.call<^a, int>(10);
  // expected-error@-1 {{expression does not reflect a data member or member function}}
  // expected-note@-2 {{in instantiation of function}}
  f.call<^b, int>(10);
  // expected-error@-1 {{expression does not reflect a data member or member function}}
  // expected-note@-2 {{in instantiation of function}}
}

} // end namespace member_function

namespace data_member {

struct foo {
  int a;

  template<auto F>
  auto get() {
    return this->[:F:];
  }
};

int a;

void test() {
  foo f;
  f.get<^foo::a>();
  f.get<^a>();
  // expected-error@-1 {{expression does not reflect a data member or member function}}
  // expected-note@-2 {{in instantiation of function}}
}

} // end namespace data_member

