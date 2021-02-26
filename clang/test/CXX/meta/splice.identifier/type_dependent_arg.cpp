// RUN: %clang_cc1 -std=c++2a -freflection -verify %s
// expected-no-diagnostics

template<typename T, T I>
class container_type {
public:
  using [# "int_alias_", I #] = int;
};

container_type<int, 1>::int_alias_1 alias_one;
