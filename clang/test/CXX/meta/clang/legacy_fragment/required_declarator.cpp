// RUN: %clang_cc1 -fsyntax-only -freflection -Wno-deprecated-fragment -std=c++2a -verify %s

namespace bad {
template<typename T, typename U>
void mismatched_types() {
  T req;
  consteval -> __fragment {
    requires U req; // expected-error {{Required declarator not found.}} // expected-note {{required declarator candidate not viable: cannot convert declarator of type ('char' to 'int')}}
  };
}

template<typename T>
void reference(T &req) {
  consteval -> __fragment {
    requires T req; // expected-error {{Required conflicting types ('int' vs 'int &')}}
  };
}

template<typename T>
void reference_2(T req) {
  consteval -> __fragment {
    requires T &req; // expected-error {{Required conflicting types ('int &' vs 'int')}}
  };
}

template<typename T>
void constexpr_mismatch() {
  constexpr T a = T(); // expected-note {{previous declaration is here}}
  T b = T(); // expected-note {{previous declaration is here}}
  consteval -> __fragment {
    requires T a; // expected-error {{non-constexpr declaration of 'a' follows constexpr declaration}}
    requires constexpr T b; // expected-error {{constexpr declaration of 'b' follows non-constexpr declaration}}
  };
}

constexpr auto frag = __fragment {
  requires int a; // expected-error {{Required declarator not found.}}
};

void future_inj() {
  consteval -> frag;
}
}; // namespace bad

void test_bad() {
  using namespace bad;
  mismatched_types<int, char>(); // expected-note {{in instantiation of function template specialization 'bad::mismatched_types<int, char>' requested here}}
  int a;
  reference<int>(a); // expected-note {{in instantiation of function template specialization 'bad::reference<int>' requested here}}
  reference_2<int>(a); // expected-note {{in instantiation of function template specialization 'bad::reference_2<int>' requested here}}
  constexpr_mismatch<int>(); // expected-note {{in instantiation of function template specialization 'bad::constexpr_mismatch<int>' requested here}}
  future_inj();
}

namespace good {
#define assert(E) if (!(E)) __builtin_abort();

template<typename T>
void matched_types() {
  T a = 0;
  consteval -> __fragment {
    requires T a;
    a = 42;
  };
  assert(a == 42);
}

constexpr auto frag = __fragment {
  requires int a;
  ++a;
};

void future_inj() {
  int a = 0;
  consteval -> frag;
  assert(a == 1);
}
}; // namespace good

void test_good() {
  using namespace good;
  matched_types<int>();
  future_inj();
}

int main() {
  test_bad();
  test_good();
}
