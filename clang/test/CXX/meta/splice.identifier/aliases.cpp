// RUN: %clang_cc1 -std=c++2a -freflection -verify %s
// expected-no-diagnostics

namespace type_aliases {

class container_type {
public:
  using unqualid("int_alias_", 1) = int;
  typedef int unqualid("int_alias_", 2);
};

container_type::int_alias_1 alias_one;
container_type::int_alias_2 alias_two;

} // end namespace type_aliases

namespace dependent_type_aliases {

template<int T>
class container_type {
public:
  using unqualid("int_alias_", T) = int;
  typedef int unqualid("int_alias_", T + 1);
};

container_type<1>::int_alias_1 alias_one;
container_type<1>::int_alias_2 alias_two;

} // end namespace dependent_type_aliases
