// RUN: %clang_cc1 -std=c++2a -freflection -Wno-deprecated-fragment -fsyntax-only -verify %s

class foo { // expected-error {{missing '}' at end of definition of 'foo'}} expected-note {{previous definition is here}}
  static constexpr auto frag = __fragment namespace {
  };

  namespace subspace { // expected-note {{still within definition of 'foo' here}}
    consteval -> frag; // expected-error {{use of undeclared identifier 'frag'}}
  }
}; // expected-error {{extraneous closing brace ('}')}}

inline namespace bar { // expected-note {{previous definition is here}}
}

consteval -> __fragment namespace {
  namespace bar {} // expected-warning {{inline namespace reopened as a non-inline namespace}}

  namespace foo {} // expected-error {{redefinition of 'foo' as different kind of symbol}}
};
