// RUN: %clang_cc1 -fcxx-exceptions -fsyntax-only -verify -Wlifetime -Wno-dangling %s

namespace {
template <class T>
class [[gsl::Owner(T *)]] unique_ptr {
public:
  unique_ptr(T * t) : t(t) {}

  T *release() {
    T *tmp = t;
    t = nullptr;
    return tmp;
  }

  T *t = nullptr;
};

class PrivateKey {
  // A random class that models a private key.
};
} // namespace

namespace nosuppressOnFunction {
PrivateKey *getPrivateKey() {
  return unique_ptr<PrivateKey>(new PrivateKey()).release(); // expected-warning {{returning a dangling pointer}}
  // expected-note@-1 {{temporary was destroyed at the end of the full expression}}
}
} // namespace nosuppressOnFunction

namespace suppressOnFunction {

[[gsl::suppress("lifetime")]] PrivateKey *getPrivateKey() {
  return unique_ptr<PrivateKey>(new PrivateKey()).release(); // OK
}

namespace suppressNotLifetime {
[[gsl::suppress("notLifetime")]] PrivateKey *getPrivateKey() {
  return unique_ptr<PrivateKey>(new PrivateKey()).release(); // expected-warning {{returning a dangling pointer}}
  // expected-note@-1 {{temporary was destroyed at the end of the full expression}}
}
} // namespace suppressNotLifetime
} // namespace suppressOnFunction