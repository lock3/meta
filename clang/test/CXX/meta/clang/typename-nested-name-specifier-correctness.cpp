// RUN: %clang_cc1 -freflection -std=c++2a %s

struct foo {
  static int i;
};

template<typename T>
int global_i = typename(reflexpr(T))::i;

int global_i_foo = global_i<foo>;

int do_things() {
  auto y = typename(reflexpr(foo))::i;
  return y;
}

template<typename T>
int templ_do_things() {
  auto y = typename(reflexpr(T))::i;
  return y;
}

int templ_do_things_foo_ret = templ_do_things<foo>();

template<typename T>
struct class_templ {
  static int templ_do_things() {
    auto y = typename(reflexpr(T))::i;
    return y;
  }
};

using class_templ_foo = class_templ<foo>;

int class_templ_foo_val = class_templ_foo::templ_do_things();
