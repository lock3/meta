// RUN: %clang_cc1 -freflection -verify -std=c++1z %s

template<int a> // expected-note {{template parameter is declared here}}
class template_a {
};

int main() {
  constexpr auto refl_a = reflexpr(template_a<int>); // expected-error {{template argument for non-type template parameter must be an expression}}
}
