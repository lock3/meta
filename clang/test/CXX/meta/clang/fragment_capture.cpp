// RUN: %clang_cc1 -fsyntax-only -verify -freflection -std=c++2a %s

class ArrayDestructure {
  consteval { // expected-error {{expression is not an integral constant expression}} expected-note {{in call to '__constexpr_decl()'}}
    int a = 0; // expected-note {{declared here}}
    int &a_ref = a;
    int *a_ptr = &a;

    -> fragment struct {
      constexpr auto val_a() {
        return %{a};
      }

      constexpr auto val_a_ref() {
        return %{a_ref};
      }

      constexpr auto val_a_ptr() {
        return %{a_ptr}; // expected-note {{pointer to 'a' is not a constant expression}}
      }
    };
  }

  consteval { // expected-error {{expression is not an integral constant expression}} expected-note {{in call to '__constexpr_decl()'}}
    auto lambda = [](auto a_ptr) consteval {
      -> fragment struct {
        constexpr auto val_a_ptr() {
          return %{a_ptr}; // expected-note {{pointer to 'a' is not a constant expression}}
        }
      };
    };

    int a = 0; // expected-note {{declared here}}
    int *a_ptr = &a;
    lambda(a_ptr); // expected-note {{in call to '&lambda->operator()(&a)'}}
  }
};

class ClassWithArr {
  int a_arr[10]; // expected-note {{subobject declared here}}
};

class BadCaptureClass {
  consteval { // expected-error {{expression is not an integral constant expression}} expected-note {{in call to '__constexpr_decl()'}}
    ClassWithArr a_arr_class;
    -> fragment struct {
      constexpr auto val_a_arr_class() {
        return %{a_arr_class}; // expected-note {{subobject of type 'int' is not initialized}}
      }
    };
  }
};
