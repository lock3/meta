// RUN: %clang_cc1 -std=c++2a -verify -freflection %s

namespace foo_ns { }

foo_ns::typename(reflexpr(int)) global; // expected-error {{the typename reifier cannot be preceded by a nested name specifier}}
