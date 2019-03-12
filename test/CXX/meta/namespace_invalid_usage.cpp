// RUN: %clang_cc1 -std=c++2a -freflection -fsyntax-only -verify %s

class foo { // expected-error {{missing '}' at end of definition of 'foo'}}
  static constexpr auto frag = __fragment namespace {
  };

  namespace subspace { // expected-note {{still within definition of 'foo' here}}
    consteval -> frag; // expected-error {{use of undeclared identifier 'frag'}}
  }
}; // expected-error {{extraneous closing brace ('}')}}
