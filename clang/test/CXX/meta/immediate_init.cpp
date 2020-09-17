// RUN: %clang_cc1 -emit-llvm -std=c++2a -freflection -o %t %s

struct foo {
  template<typename T>
  consteval foo(T var) {
  }
};

template<typename T>
T init_via_template() {
  return T(reflexpr(foo));
}

template<typename T>
foo init_with_template_arg() {
  return foo(reflexpr(T));
}

int main() {
  foo constructed_val = foo(reflexpr(foo));
  foo template_constructed_val = init_via_template<foo>();
  foo template_arg_constructed_val = init_with_template_arg<foo>();
  return 0;
}
