// RUN: %clang_cc1 -verify -fsyntax-only -freflection -Wno-deprecated-fragment -std=c++2a %s

class C {
  void foo() { } // expected-note {{previous definition is here}}
  void bar() { } // expected-note {{previous definition is here}}

  consteval -> __fragment class {
    void foo() { } // expected-error {{class member cannot be redeclared}}
    int bar() { return 0; } // expected-error {{functions that differ only in their return type cannot be overloaded}}
  };

  int foo_var; // expected-note {{previous declaration is here}}
  int bar_var; // expected-note {{previous declaration is here}}

  consteval -> __fragment class {
    int foo_var; // expected-error {{duplicate member 'foo_var'}}
    float bar_var; // expected-error {{duplicate member 'bar_var'}}
  };
};

void foo() { } // expected-note {{previous definition is here}}
void bar() { } // expected-note {{previous definition is here}}

consteval -> __fragment namespace {
  void foo();
  void foo() { } // expected-error {{redefinition of 'foo'}}
  int bar() { return 0; } // expected-error {{functions that differ only in their return type cannot be overloaded}}
};

int foo_var; // expected-note {{previous definition is here}}
int bar_var; // expected-note {{previous definition is here}}

consteval -> __fragment namespace {
  int foo_var; // expected-error {{redefinition of 'foo_var'}}
  float bar_var; // expected-error {{redefinition of 'bar_var' with a different type: 'float' vs 'int'}}
};

class Foo { // expected-note {{previous definition is here}}
  class Bar; // expected-note {{previous declaration is here}}
  consteval -> __fragment class {
    class Bar; // expected-warning {{class member cannot be redeclared}}
  };

  enum EFoo { }; // expected-note {{previous definition is here}}
  enum EFooSized : unsigned; // expected-note {{previous declaration is here}}
  consteval -> __fragment class {
    enum EFoo { }; // expected-error {{redefinition of 'EFoo'}}
    enum EFooSized : unsigned; // expected-warning  {{class member cannot be redeclared}}
  };
};

consteval -> __fragment namespace {
  class Foo { //expected-error {{redefinition of 'Foo'}}
  };
};

enum EFoo { // expected-note {{previous definition is here}}
};

consteval -> __fragment namespace {
  enum EFoo { // expected-error {{redefinition of 'EFoo'}}
  };
};
