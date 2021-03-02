// RUN: %clang_cc1 -std=c++2a -freflection -verify %s
// expected-no-diagnostics

using info = decltype(^void);

struct x {
  consteval {
    -> fragment class {
      consteval {
        info z = ^int;
      }
    };
  }
};
