// RUN: %clang_cc1 -std=c++2a -freflection -verify %s

using info = decltype(^void);

namespace unexpanded {

template<typename... Ts>
void bar() { }
// expected-note@-1 {{candidate template ignored: invalid explicitly-specified argument for template parameter 'Ts'}}

void foo() {
  constexpr info els [] = { ^int, ^int };
  bar<...[: els :]>();
// expected-error@-1 {{template argument contains an unexpanded pack splice}}
// expected-error@-2 {{no matching function for call to 'bar'}}
}

void baz() {
  constexpr info els [] = { ^int, ^int };
  using X = ...[: els :];
// expected-error@-1 {{declaration type contains an unexpanded parameter pack}}
}

}

namespace expanded {

template<int... Ts>
constexpr int total() {
  return (... + Ts);
}

struct tri_construct {
  int total;

  constexpr tri_construct(int x, int y, int z) : total(x + y + z) { }
};

template<typename T, int X>
constexpr auto construct_the_thing() {
  return T(X);
}

struct base_a_base {
  constexpr bool base_a_is_present() { return true; }
};
struct base_b_base {
  constexpr bool base_b_is_present() { return true; }
};

constexpr info base_types [] = { ^base_a_base, ^base_b_base };

namespace dependent {

template<int I>
void template_argument_list() {
  constexpr info els [] = { ^I, ^1, ^0 };
  static_assert(total<...[: els :]...>() == 2);
}

template<int I>
void function_call() {
  constexpr info els [] = { ^I, ^1, ^0 };
  static_assert(tri_construct(...[: els :]...).total == 2);
}

template<int I>
void initializer_list() {
  constexpr info els [] = { ^I, ^1, ^0 };

  constexpr tri_construct init_list = { ...[: els :]... };
  static_assert(init_list.total == 2);
}

template<int I>
void mixed_pack() {
  constexpr info construct_args [] = { ^int, ^I };
  static_assert(construct_the_thing<...[: construct_args :]...>() == 1);
}

template<info X>
struct base_class : public ...[: [: X :] :]... { };

template<int I>
void base_splice() {
  base_class<^base_types> base_inst;
  static_assert(base_inst.base_a_is_present());
  static_assert(base_inst.base_b_is_present());
}

template<info X>
struct constructed_base_class : public ...[: [: X :] :]... {
  constructed_base_class(const ...[: [: X :] :]&... args)
    : ...[: [: X :] :](args)... {
  }
};

template<int I>
void constructed_base_splice() {
  constructed_base_class<^base_types> base_inst {
      base_a_base(), base_b_base()
  };
}

void instantiate() {
  template_argument_list<1>();
  function_call<1>();
  initializer_list<1>();
  mixed_pack<1>();
  base_splice<1>();
  constructed_base_splice<1>();
}

} // end namespace dependent

namespace non_dependent {

void template_argument_list() {
  constexpr info els [] = { ^1, ^1, ^0 };
  static_assert(total<...[: els :]...>() == 2);
}

void function_call() {
  constexpr info els [] = { ^1, ^1, ^0 };
  static_assert(tri_construct(...[: els :]...).total == 2);
}

void initializer_list() {
  constexpr info els [] = { ^1, ^1, ^0 };

  constexpr tri_construct init_list = { ...[: els :]... };
  static_assert(init_list.total == 2);
}

void mixed_pack() {
  constexpr info construct_args [] = { ^int, ^1 };
  static_assert(construct_the_thing<...[: construct_args :]...>() == 1);
}

struct base_class : public ...[: base_types :]... { };

void base_splice() {
  base_class base_inst;
  static_assert(base_inst.base_a_is_present());
  static_assert(base_inst.base_b_is_present());
}

struct constructed_base_class : public ...[: base_types :]... {
  constructed_base_class(const ...[: base_types :]&... args)
    : ...[: base_types :](args)... {
  }
};

void constructed_base_splice() {
  constructed_base_class base_inst { base_a_base(), base_b_base() };
}

} // end namespace non_dependent

} // end namespace expanded
