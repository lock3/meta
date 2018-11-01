
struct assertion { };

#define assert(E) if (!(E)) __builtin_abort();

// FIXME: This needs to be kept in sync with AST/Reflction.h.
enum reflection_query {
  query_unknown,

  query_is_invalid,
  query_is_entity,
  query_is_unnamed,

  // Objects, references, bitfields, and functions
  query_is_variable,
  query_is_enumerator,
  query_is_static_data_member,
  query_is_static_member_function,
  query_is_nonstatic_data_member,
  query_is_bitfield,
  query_is_nonstatic_member_function,
  query_is_constructor,
  query_is_destructor,
  
  // Types
  query_is_type,
  query_is_function,
  query_is_class,
  query_is_union,
  query_is_enum,
  query_is_scoped_enum,
  query_is_void,
  query_is_null_pointer,
  query_is_integral,
  query_is_floating_point,
  query_is_array,
  query_is_pointer,
  query_is_lvalue_reference,
  query_is_rvalue_reference,
  query_is_member_object_pointer,
  query_is_member_function_pointer,
  query_is_closure,

  // Namespaces and aliases
  query_is_namespace,
  query_is_namespace_alias,
  query_is_type_alias,

  // Templates and specializations
  query_is_template,
  query_is_class_template,
  query_is_alias_template,
  query_is_function_template,
  query_is_variable_template,
  query_is_member_function_template,
  query_is_static_member_function_template,
  query_is_nonstatic_member_function_template,
  query_is_constructor_template,
  query_is_destructor_template,
  query_is_concept,
  query_is_specialization,
  query_is_partial_specialization,
  query_is_explicit_specialization,
  query_is_implicit_instantiation,
  query_is_explicit_instantiation,

  // Base class specifiers
  query_is_direct_base,
  query_is_virtual_base,

  // Parameters
  query_is_function_parameter,
  query_is_template_parameter,
  query_is_type_template_parameter,
  query_is_nontype_template_parameter,
  query_is_template_template_parameter,

  // Expressions
  query_is_expression,
  query_is_lvalue,
  query_is_xvalue,
  query_is_rvalue,

  // Scope
  query_is_local,
  query_is_class_member,
  
  // Traits
  query_get_variable_traits,
  query_get_function_traits,
  query_get_namespace_traits,
  query_get_linkage_traits,
  query_get_access_traits,

  // Associated reflections
  query_get_entity,
  query_get_parent,
  query_get_type,
  query_get_this_ref_type,
  
  // Traversal
  query_get_begin,
  query_get_next,

  // Name
  query_get_name,
  query_get_display_name,
};



struct S {
  enum E { X, Y };
  enum class EC { X, Y };
};

enum E { A, B, C };
enum class EC { A, B, C };

void f() { }

int global;

void ovl();
void ovl(int);

template<typename T>
void fn_tmpl() { }

template<typename T>
struct Template { };

namespace N { 
  namespace M { 
    enum E { A, B, C };
    enum class EC { A, B, C };
  }
}

constexpr int test() {
  // FIXME: 
  // auto r0 = reflexpr(::);
  // assert(__reflect(query_is_namespace(r0)));

  auto r1 = reflexpr(S);
  assert(__reflect(query_is_type, r1));
  assert(__reflect(query_is_class, r1));
  assert(!__reflect(query_is_namespace, r1));

  auto r2 = reflexpr(E);
  assert(__reflect(query_is_type, r2));
  assert(__reflect(query_is_enum, r2));
  
  auto r3 = reflexpr(A);
  assert(__reflect(query_is_expression, r3));
  assert(__reflect(query_is_enumerator, r3));
  
  auto r4 = reflexpr(EC);
  assert(__reflect(query_is_type, r4));
  assert(__reflect(query_is_enum, r4));
  assert(__reflect(query_is_scoped_enum, r4));

  auto r5 = reflexpr(EC::A);
  assert(__reflect(query_is_expression, r5));
  assert(__reflect(query_is_enumerator, r5));

  auto r6 = reflexpr(f);
  assert(__reflect(query_is_expression, r6));
  assert(__reflect(query_is_function, r6));

  auto r7 = reflexpr(global);

  auto r8 = reflexpr(ovl);

  auto r9 = reflexpr(fn_tmpl);

  auto r10 = reflexpr(Template);

  auto r11 = reflexpr(N);
  assert(__reflect(query_is_namespace, r11));
  
  auto r12 = reflexpr(N::M);

  int N = 0;
  auto r12a = reflexpr(N); // Finds int N, not namespace N

  auto r13 = reflexpr(S::E);
  // assert(__reflect(query_get_parent, r13) == reflexpr(S));
  (void)__reflect(query_get_parent, r13);

  auto r14 = reflexpr(S::X);
  (void)__reflect(query_get_type, r14);
  
  auto r15 = reflexpr(S::EC);
  
  auto r16 = reflexpr(S::EC::X);

  auto r17 = reflexpr(N::M::E);
  
  auto r18 = reflexpr(N::M::A);

  auto r19 = reflexpr(N::M::EC);

  auto r20 = reflexpr(N::M::EC::A);

  return 0;
}

template<typename T>
constexpr int type_template() {
  auto r = reflexpr(T);
  return 0;
}

template<int N>
constexpr int nontype_template() {
  auto r = reflexpr(N);
  return 0;
}

template<template<typename> class X>
constexpr int template_template() {
  auto r = reflexpr(X);
  return 0;
}

template<typename T>
struct temp { };

int main() {
  constexpr int n = test();

  constexpr int t = type_template<int>();
  constexpr int v = nontype_template<0>();
  constexpr int x = template_template<temp>();
}
