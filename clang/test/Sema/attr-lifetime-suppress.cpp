// RUN: %clang_cc1 -fcxx-exceptions -fsyntax-only -verify -Wlifetime -Wno-dangling %s

namespace nosuppressOnFunction {
int *f() {
  int *p; // expected-note {{it was never initialized here}}
  return p; // expected-warning {{returning a dangling pointer}}
}
} // namespace nosuppressOnFunction

namespace suppressOnFunction {

[[gsl::suppress("lifetime")]] int *f() {
  int *p;
  return p; // OK
}

namespace suppressNotLifetime {
[[gsl::suppress("notLifetime")]] int *f() {
  int *p; // expected-note {{it was never initialized here}}
  return p; // expected-warning {{returning a dangling pointer}}
}
} // namespace suppressNotLifetime
} // namespace suppressOnFunction