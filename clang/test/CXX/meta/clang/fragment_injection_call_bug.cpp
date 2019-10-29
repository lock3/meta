// RUN: %clang_cc1 -freflection -std=c++2a -fsyntax-only -verify %s

constexpr auto frag = __fragment {
  return 0;
};

int main() {
  consteval -> frag(); // expected-error-re {{type 'const (fragment at {{.*}}fragment_injection_call_bug.cpp:{{[0-9]+}}:{{[0-9]+}})' does not provide a call operator}}
  return 0;
}
