// RUN: %clang_cc1 -freflection -std=c++2a %s

struct foo {
  constexpr static int i = 10;
};

template<typename T>
constexpr int global_i = typename(reflexpr(T))::i;
static_assert(global_i<foo> == 10);

constexpr int do_things() {
  auto y = typename(reflexpr(foo))::i;
  return y + 5;
}

constexpr int do_things_foo_ret = do_things();
static_assert(do_things_foo_ret == 15);

template<typename T>
constexpr int templ_do_things() {
  auto y = typename(reflexpr(T))::i;
  return y + 10;
}

constexpr int templ_do_things_foo_ret = templ_do_things<foo>();
static_assert(templ_do_things_foo_ret == 20);

template<typename T>
struct class_templ {
  constexpr static int templ_do_things() {
    auto y = typename(reflexpr(T))::i;
    return y + 15;
  }
};

using class_templ_foo = class_templ<foo>;

constexpr int class_templ_foo_val = class_templ_foo::templ_do_things();
static_assert(class_templ_foo_val == 25);
