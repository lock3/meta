// RUN: %clang_cc1 -std=c++2a -verify -freflection %s

namespace foo_ns { }

foo_ns::typename(^int) global; // expected-error {{expected unqualified-id}}
