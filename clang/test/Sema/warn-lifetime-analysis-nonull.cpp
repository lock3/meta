// RUN: %clang_cc1 -fsyntax-only -std=c++11 -verify -Wlifetime -Wno-lifetime-null %s

void deref_uninitialized() {
  int *p;        // expected-note {{it was never initialized here}}
  *p = 3;        // expected-warning {{dereferencing a dangling pointer}}
}

void deref_nullptr() {
  int *q = nullptr;
  *q = 3;           // No warning due to -Wno-lifetime-null
}