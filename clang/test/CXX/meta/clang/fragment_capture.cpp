// RUN: %clang_cc1 -fsyntax-only -verify -freflection -std=c++2a %s

class ArrayDestructure {
  consteval {
    auto lambda = [](auto a_ptr) // expected-error {{invalid deduced capture; 'int *' cannot be captured by a fragment}}
      consteval {
      -> __fragment struct {
        constexpr auto val_a_ptr() {
          return a_ptr;
        }
      };
    };

    int a = 0;
    int &a_ref = a; // expected-note {{'a_ref' declared here}}
    int *a_ptr = &a; // expected-note {{'a_ptr' declared here}}

    -> __fragment struct {
      constexpr auto val_a() {
        return a;
      }

      constexpr auto val_a_ref() {
        return a_ref; // expected-error {{reference to local variable 'a_ref' declared in enclosing function 'ArrayDestructure::(consteval block)'}}
      }

      constexpr auto val_a_ptr() {
        return a_ptr; // expected-error {{reference to local variable 'a_ptr' declared in enclosing function 'ArrayDestructure::(consteval block)'}}
      }
    };
    lambda(a_ptr); // expected-note {{in instantiation of function template specialization}}
  }
};

class ClassWithArr {
  int a_arr[10]; // expected-note {{subobject declared here}}
};

class BadCaptureClass {
  consteval { // expected-error {{expression is not an integral constant expression}} expected-note {{in call to '(consteval block)'}}
    ClassWithArr a_arr_class; // expected-note {{subobject of type 'int' is not initialized}} expected-note {{in call to 'ClassWithArr(a_arr_class)'}}
    -> __fragment struct {
      constexpr auto val_a_arr_class() {
        return a_arr_class;
      }
    };
  }
};
