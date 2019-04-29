// RUN: %clang_cc1 -verify -fsyntax-only -freflection -std=c++1z %s

class C {
  void foo() { } // expected-note {{previous definition is here}}
  void bar() { } // expected-note {{previous definition is here}}

  consteval -> __fragment class {
    void foo() { } // expected-error {{class member cannot be redeclared}}
    int bar() { return 0; } // expected-error {{functions that differ only in their return type cannot be overloaded}}
  }
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
