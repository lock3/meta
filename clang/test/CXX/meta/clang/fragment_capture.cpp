// RUN: %clang_cc1 -fsyntax-only -verify -freflection -std=c++2a %s

class ArrayDestructure {
  consteval {
    int a = 0;
    int &a_ref = a; // expected-note {{'a_ref' declared here}}
    int *a_ptr = &a; // expected-note {{'a_ptr' declared here}}
    -> __fragment struct {
      constexpr auto val_a() {
        return a;
      }

      constexpr auto val_a_ref() {
        return a_ref; // expected-error {{reference to local variable 'a_ref' declared in enclosing function 'ArrayDestructure::__constexpr_decl'}}
      }

      constexpr auto val_a_ptr() {
        return a_ptr; // expected-error {{reference to local variable 'a_ptr' declared in enclosing function 'ArrayDestructure::__constexpr_decl'}}
      }
    };
  }
};

class ClassWithArr {
  int a_arr[10]; // expected-note {{subobject declared here}}
};

class BadCaptureClass {
  consteval { // expected-error {{expression is not an integral constant expression}} expected-note {{in call to '__constexpr_decl()'}}
    ClassWithArr a_arr_class; // expected-note {{subobject of type 'int' is not initialized}} expected-note {{in call to 'ClassWithArr(a_arr_class)'}}
    -> __fragment struct {
      constexpr auto val_a_arr_class() {
        return a_arr_class;
      }
    };
  }
};
