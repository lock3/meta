// RUN: %clang_cc1 -freflection -std=c++1z %s

#include "reflection_query.h"

#define assert(E) if (!(E)) __builtin_abort();

int x1;
static int x2;

void f1() { }
void f2();
static inline void f3() { }
constexpr int f4() { return 0; }
void f5(int) = delete;

class C1 { };

class C2;

union U1 { };

enum E1 { X };
enum class E2 { X };
enum class E3 : int;


struct S1 {
  int x1;
  mutable int x2;
  static int x3;
  static constexpr int x4 = 0;
  int x5 : 4;

  void f1() { }
  constexpr int f2() const { return 42; }

  class C { };

  void f3();
private:
  int y1;
};

struct S2 {
  static void f1() { }
};

namespace N {
  inline namespace M { }
}

void S1::f3()
{
  constexpr auto r = reflexpr(y1);
  // assert(kind(r) == meta::data_member_decl);

  assert(__reflect(query_is_null_pointer, r) == false);
  // assert(is_declaration(r) == true);
  assert(__reflect(query_is_type, r) == false);
  // assert(is_translation_unit(r) == false);
  assert(__reflect(query_is_namespace, r) == false);
  assert(__reflect(query_is_variable, r) == false);
  assert(__reflect(query_is_function, r) == false);
  assert(__reflect(query_is_function_parameter, r) == false);
  assert(__reflect(query_is_class, r) == false);
  assert(__reflect(query_is_nonstatic_data_member, r) == true);
  assert(__reflect(query_is_nonstatic_member_function, r) == false);
  // assert(is_access_specifier(r) == false);
  assert(__reflect(query_is_enum, r) == false);
  assert(__reflect(query_is_enumerator, r) == false);

  // assert(is_public(r) == false);
  assert(__reflect(query_is_static_data_member, r) == false);
  assert(__reflect(query_is_static_member_function, r) == false);
  // assert(is_mutable(r) == false);
  // assert(is_inline(r) == false);
  // assert(is_constexpr(r) == false);
  assert(__reflect(query_is_bitfield, r) == false);

}

int main(int argc, char* argv[]) {

  // int x1 -- global
  {
    constexpr auto r = reflexpr(x1);
    // assert(kind(r) == meta::variable_decl);

    assert(__reflect(query_is_null_pointer, r) == false);
    // assert(is_declaration(r) == true);
    assert(__reflect(query_is_type, r) == false);
    // assert(is_translation_unit(r) == false);
    assert(__reflect(query_is_namespace, r) == false);
    assert(__reflect(query_is_variable, r) == true);
    assert(__reflect(query_is_function, r) == false);
    assert(__reflect(query_is_function_parameter, r) == false);
    assert(__reflect(query_is_class, r) == false);
    assert(__reflect(query_is_nonstatic_data_member, r) == false);
    assert(__reflect(query_is_nonstatic_member_function, r) == false);
    // assert(is_access_specifier(r) == false);
    assert(__reflect(query_is_enum, r) == false);
    assert(__reflect(query_is_enumerator, r) == false);

    // assert(has_external_linkage(r) == true);
    // assert(has_static_storage(r) == true);
    // assert(is_extern(r) == false);
    assert(__reflect(query_is_static_data_member, r) == false);
    assert(__reflect(query_is_static_member_function, r) == false);
    // assert(is_inline(r) == false);
    // assert(is_constexpr(r) == false);
  }

  // static int x2 -- global
  {
    constexpr auto r = reflexpr(x2);
    // assert(kind(r) == meta::variable_decl);

    assert(__reflect(query_is_null_pointer, r) == false);
    // assert(is_declaration(r) == true);
    assert(__reflect(query_is_type, r) == false);
    // assert(is_translation_unit(r) == false);
    assert(__reflect(query_is_namespace, r) == false);
    assert(__reflect(query_is_variable, r) == true);
    assert(__reflect(query_is_function, r) == false);
    assert(__reflect(query_is_function_parameter, r) == false);
    assert(__reflect(query_is_class, r) == false);
    assert(__reflect(query_is_nonstatic_data_member, r) == false);
    assert(__reflect(query_is_nonstatic_member_function, r) == false);
    // assert(is_access_specifier(r) == false);
    assert(__reflect(query_is_enum, r) == false);
    assert(__reflect(query_is_enumerator, r) == false);

    // assert(has_internal_linkage(r) == true);
    // assert(has_static_storage(r) == true);
    // assert(is_extern(r) == false);
    assert(__reflect(query_is_static_data_member, r) == true);
    assert(__reflect(query_is_static_member_function, r) == false);
    // assert(is_inline(r) == false);
    // assert(is_constexpr(r) == false);
  }

  // Functions

  // void f1() { }
  {
    constexpr auto r = reflexpr(f1);
    // assert(kind(r) == meta::function_decl);

    assert(__reflect(query_is_null_pointer, r) == false);
    // assert(is_declaration(r) == true);
    assert(__reflect(query_is_type, r) == false);
    // assert(is_translation_unit(r) == false);
    assert(__reflect(query_is_namespace, r) == false);
    assert(__reflect(query_is_variable, r) == false);
    assert(__reflect(query_is_function, r) == true);
    assert(__reflect(query_is_function_parameter, r) == false);
    assert(__reflect(query_is_class, r) == false);
    assert(__reflect(query_is_nonstatic_data_member, r) == false);
    assert(__reflect(query_is_nonstatic_member_function, r) == false);
    // assert(is_access_specifier(r) == false);
    assert(__reflect(query_is_enum, r) == false);
    assert(__reflect(query_is_enumerator, r) == false);

    // assert(has_external_linkage(r) == true);
    assert(__reflect(query_is_static_data_member, r) == false);
    assert(__reflect(query_is_static_member_function, r) == false);
    // assert(is_extern(r) == false);
    // assert(is_constexpr(r) == false);
    // assert(is_defined(r) == true);
    // assert(is_inline(r) == false);
    // assert(is_deleted(r) == false);
  }

  // void f2();
  {
    constexpr auto r = reflexpr(f2);
    // assert(kind(r) == meta::function_decl);

    assert(__reflect(query_is_null_pointer, r) == false);
    // assert(is_declaration(r) == true);
    assert(__reflect(query_is_type, r) == false);
    // assert(is_translation_unit(r) == false);
    assert(__reflect(query_is_namespace, r) == false);
    assert(__reflect(query_is_variable, r) == false);
    assert(__reflect(query_is_function, r) == true);
    assert(__reflect(query_is_function_parameter, r) == false);
    assert(__reflect(query_is_class, r) == false);
    assert(__reflect(query_is_nonstatic_data_member, r) == false);
    assert(__reflect(query_is_nonstatic_member_function, r) == false);
    // assert(is_access_specifier(r) == false);
    assert(__reflect(query_is_enum, r) == false);
    assert(__reflect(query_is_enumerator, r) == false);

    // assert(has_external_linkage(r) == true);
    assert(__reflect(query_is_static_data_member, r) == false);
    assert(__reflect(query_is_static_member_function, r) == false);
    // assert(is_extern(r) == false);
    // assert(is_constexpr(r) == false);
    // assert(is_defined(r) == false);
  }

  // static inline void f3() { }
  {
    constexpr auto r = reflexpr(f3);
    // assert(kind(r) == meta::function_decl);

    assert(__reflect(query_is_null_pointer, r) == false);
    // assert(is_declaration(r) == true);
    assert(__reflect(query_is_type, r) == false);
    // assert(is_translation_unit(r) == false);
    assert(__reflect(query_is_namespace, r) == false);
    assert(__reflect(query_is_variable, r) == false);
    assert(__reflect(query_is_function, r) == true);
    assert(__reflect(query_is_function_parameter, r) == false);
    assert(__reflect(query_is_class, r) == false);
    assert(__reflect(query_is_nonstatic_data_member, r) == false);
    assert(__reflect(query_is_nonstatic_member_function, r) == false);
    // assert(is_access_specifier(r) == false);
    assert(__reflect(query_is_enum, r) == false);
    assert(__reflect(query_is_enumerator, r) == false);

    // assert(has_internal_linkage(r) == true);
    assert(__reflect(query_is_static_data_member, r) == false);
    assert(__reflect(query_is_static_member_function, r) == true);
    // assert(is_extern(r) == false);
    // assert(is_constexpr(r) == false);
    // assert(is_defined(r) == true);
    // assert(is_inline(r) == true);
    // assert(is_deleted(r) == false);
  }

  // constexpr int f4() { return 0; }
  {
    constexpr auto r = reflexpr(f4);
    // assert(kind(r) == meta::function_decl);

    assert(__reflect(query_is_null_pointer, r) == false);
    // assert(is_declaration(r) == true);
    assert(__reflect(query_is_type, r) == false);
    // assert(is_translation_unit(r) == false);
    assert(__reflect(query_is_namespace, r) == false);
    assert(__reflect(query_is_variable, r) == false);
    assert(__reflect(query_is_function, r) == true);
    assert(__reflect(query_is_function_parameter, r) == false);
    assert(__reflect(query_is_class, r) == false);
    assert(__reflect(query_is_nonstatic_data_member, r) == false);
    assert(__reflect(query_is_nonstatic_member_function, r) == false);
    // assert(is_access_specifier(r) == false);
    assert(__reflect(query_is_enum, r) == false);
    assert(__reflect(query_is_enumerator, r) == false);

    // assert(has_external_linkage(r) == true);
    assert(__reflect(query_is_static_data_member, r) == false);
    assert(__reflect(query_is_static_member_function, r) == false);
    // assert(is_extern(r) == false);
    // assert(is_constexpr(r) == true);
    // assert(is_defined(r) == true);
    // assert(is_inline(r) == true);
    // assert(is_deleted(r) == false);
  }

  // void f5(int) = delete;
  {
    constexpr auto r = reflexpr(f5);
    // assert(kind(r) == meta::function_decl);

    assert(__reflect(query_is_null_pointer, r) == false);
    // assert(is_declaration(r) == true);
    assert(__reflect(query_is_type, r) == false);
    // assert(is_translation_unit(r) == false);
    assert(__reflect(query_is_namespace, r) == false);
    assert(__reflect(query_is_variable, r) == false);
    assert(__reflect(query_is_function, r) == true);
    assert(__reflect(query_is_function_parameter, r) == false);
    assert(__reflect(query_is_class, r) == false);
    assert(__reflect(query_is_nonstatic_data_member, r) == false);
    assert(__reflect(query_is_nonstatic_member_function, r) == false);
    // assert(is_access_specifier(r) == false);
    assert(__reflect(query_is_enum, r) == false);
    assert(__reflect(query_is_enumerator, r) == false);

    // assert(has_external_linkage(r) == true);
    assert(__reflect(query_is_static_data_member, r) == false);
    assert(__reflect(query_is_static_member_function, r) == false);
    // assert(is_extern(r) == false);
    // assert(is_constexpr(r) == false);
    // assert(is_defined(r) == true);
    // assert(is_inline(r) == true); // FIXME: Apparently so.
    // assert(is_deleted(r) == true);
  }

  // Namespaces

  // namespace N
  {
    constexpr auto r = reflexpr(N);
    // assert(kind(r) == meta::namespace_decl);

    assert(__reflect(query_is_null_pointer, r) == false);
    // assert(is_declaration(r) == true);
    assert(__reflect(query_is_type, r) == false);
    // assert(is_translation_unit(r) == false);
    assert(__reflect(query_is_namespace, r) == true);
    assert(__reflect(query_is_variable, r) == false);
    assert(__reflect(query_is_function, r) == false);
    assert(__reflect(query_is_function_parameter, r) == false);
    assert(__reflect(query_is_class, r) == false);
    assert(__reflect(query_is_nonstatic_data_member, r) == false);
    assert(__reflect(query_is_nonstatic_member_function, r) == false);
    // assert(is_access_specifier(r) == false);
    assert(__reflect(query_is_enum, r) == false);
    assert(__reflect(query_is_enumerator, r) == false);

    // assert(is_inline(r) == false);
  }

  // inline namespace N::M
  {
    constexpr auto r = reflexpr(N::M);
    // assert(kind(r) == meta::namespace_decl);

    assert(__reflect(query_is_null_pointer, r) == false);
    // assert(is_declaration(r) == true);
    assert(__reflect(query_is_type, r) == false);
    // assert(is_translation_unit(r) == false);
    assert(__reflect(query_is_namespace, r) == true);
    assert(__reflect(query_is_variable, r) == false);
    assert(__reflect(query_is_function, r) == false);
    assert(__reflect(query_is_function_parameter, r) == false);
    assert(__reflect(query_is_class, r) == false);
    assert(__reflect(query_is_nonstatic_data_member, r) == false);
    assert(__reflect(query_is_nonstatic_member_function, r) == false);
    // assert(is_access_specifier(r) == false);
    assert(__reflect(query_is_enum, r) == false);
    assert(__reflect(query_is_enumerator, r) == false);

    // assert(is_inline(r) == true);
  }

  // Classes

  // class C1 { };
  {
    constexpr auto r = reflexpr(C1);
    // assert(kind(r) == meta::class_decl);

    assert(__reflect(query_is_null_pointer, r) == false);
    // assert(is_declaration(r) == true);
    assert(__reflect(query_is_type, r) == false);
    // assert(is_translation_unit(r) == false);
    assert(__reflect(query_is_namespace, r) == false);
    assert(__reflect(query_is_variable, r) == false);
    assert(__reflect(query_is_function, r) == false);
    assert(__reflect(query_is_function_parameter, r) == false);
    assert(__reflect(query_is_class, r) == true);
    assert(__reflect(query_is_nonstatic_data_member, r) == false);
    assert(__reflect(query_is_nonstatic_member_function, r) == false);
    // assert(is_access_specifier(r) == false);
    assert(__reflect(query_is_enum, r) == false);
    assert(__reflect(query_is_enumerator, r) == false);

    // assert(has_external_linkage(r) == true);
    // assert(has_access(r) == false);
    assert(__reflect(query_is_class, r) == true);
    // assert(is_declared_class(r) == true);
    // assert(is_complete(r) == true);
  }

  // class C2;
  {
    constexpr auto r = reflexpr(C2);
    // assert(kind(r) == meta::class_decl);

    assert(__reflect(query_is_null_pointer, r) == false);
    // assert(is_declaration(r) == true);
    assert(__reflect(query_is_type, r) == false);
    // assert(is_translation_unit(r) == false);
    assert(__reflect(query_is_namespace, r) == false);
    assert(__reflect(query_is_variable, r) == false);
    assert(__reflect(query_is_function, r) == false);
    assert(__reflect(query_is_function_parameter, r) == false);
    assert(__reflect(query_is_class, r) == true);
    assert(__reflect(query_is_nonstatic_data_member, r) == false);
    assert(__reflect(query_is_nonstatic_member_function, r) == false);
    // assert(is_access_specifier(r) == false);
    assert(__reflect(query_is_enum, r) == false);
    assert(__reflect(query_is_enumerator, r) == false);

    // assert(has_external_linkage(r) == true);
    // assert(has_access(r) == false);
    assert(__reflect(query_is_class, r) == true);
    // assert(is_complete(r) == false);
  }

  // struct S1 { ... };
  {
    constexpr auto r = reflexpr(S1);
    // assert(kind(r) == meta::class_decl);

    assert(__reflect(query_is_null_pointer, r) == false);
    // assert(is_declaration(r) == true);
    assert(__reflect(query_is_type, r) == false);
    // assert(is_translation_unit(r) == false);
    assert(__reflect(query_is_namespace, r) == false);
    assert(__reflect(query_is_variable, r) == false);
    assert(__reflect(query_is_function, r) == false);
    assert(__reflect(query_is_function_parameter, r) == false);
    assert(__reflect(query_is_class, r) == true);
    assert(__reflect(query_is_nonstatic_data_member, r) == false);
    assert(__reflect(query_is_nonstatic_member_function, r) == false);
    // assert(is_access_specifier(r) == false);
    assert(__reflect(query_is_enum, r) == false);
    assert(__reflect(query_is_enumerator, r) == false);

    // assert(is_declared_struct(r) == true);
  }

  // class S1::C { ... };
  {
    constexpr auto r = reflexpr(S1::C);
    // assert(kind(r) == meta::class_decl);

    assert(__reflect(query_is_null_pointer, r) == false);
    // assert(is_declaration(r) == true);
    assert(__reflect(query_is_type, r) == false);
    // assert(is_translation_unit(r) == false);
    assert(__reflect(query_is_namespace, r) == false);
    assert(__reflect(query_is_variable, r) == false);
    assert(__reflect(query_is_function, r) == false);
    assert(__reflect(query_is_function_parameter, r) == false);
    assert(__reflect(query_is_class, r) == true);
    assert(__reflect(query_is_nonstatic_data_member, r) == false);
    assert(__reflect(query_is_nonstatic_member_function, r) == false);
    // assert(is_access_specifier(r) == false);
    assert(__reflect(query_is_enum, r) == false);
    assert(__reflect(query_is_enumerator, r) == false);

    // assert(has_access(r) == true);
    // assert(is_public(r) == true);
  }

  // union U1 { ... };
  {
    constexpr auto r = reflexpr(U1);
    // assert(kind(r) == meta::class_decl);

    assert(__reflect(query_is_null_pointer, r) == false);
    // assert(is_declaration(r) == true);
    assert(__reflect(query_is_type, r) == false);
    // assert(is_translation_unit(r) == false);
    assert(__reflect(query_is_namespace, r) == false);
    assert(__reflect(query_is_variable, r) == false);
    assert(__reflect(query_is_function, r) == false);
    assert(__reflect(query_is_function_parameter, r) == false);
    assert(__reflect(query_is_class, r) == true);
    assert(__reflect(query_is_nonstatic_data_member, r) == false);
    assert(__reflect(query_is_nonstatic_member_function, r) == false);
    // assert(is_access_specifier(r) == false);
    assert(__reflect(query_is_enum, r) == false);
    assert(__reflect(query_is_enumerator, r) == false);

    assert(__reflect(query_is_union, r) == true);
  }

  // Data members

  // int S1::x1;
  {
    constexpr auto r = reflexpr(S1::x1);
    // assert(kind(r) == meta::data_member_decl);

    assert(__reflect(query_is_null_pointer, r) == false);
    // assert(is_declaration(r) == true);
    assert(__reflect(query_is_type, r) == false);
    // assert(is_translation_unit(r) == false);
    assert(__reflect(query_is_namespace, r) == false);
    assert(__reflect(query_is_variable, r) == false);
    assert(__reflect(query_is_function, r) == false);
    assert(__reflect(query_is_function_parameter, r) == false);
    assert(__reflect(query_is_class, r) == false);
    assert(__reflect(query_is_nonstatic_data_member, r) == true);
    assert(__reflect(query_is_nonstatic_member_function, r) == false);
    // assert(is_access_specifier(r) == false);
    assert(__reflect(query_is_enum, r) == false);
    assert(__reflect(query_is_enumerator, r) == false);

    // assert(is_public(r) == true);
    assert(__reflect(query_is_static_data_member, r) == false);
    assert(__reflect(query_is_static_member_function, r) == false);
    // assert(is_mutable(r) == false);
    // assert(is_inline(r) == false);
    // assert(is_constexpr(r) == false);
    assert(__reflect(query_is_bitfield, r) == false);
  }

  // mutable int S1::x2;
  {
    constexpr auto r = reflexpr(S1::x2);
    // assert(kind(r) == meta::data_member_decl);

    assert(__reflect(query_is_null_pointer, r) == false);
    // assert(is_declaration(r) == true);
    assert(__reflect(query_is_type, r) == false);
    // assert(is_translation_unit(r) == false);
    assert(__reflect(query_is_namespace, r) == false);
    assert(__reflect(query_is_variable, r) == false);
    assert(__reflect(query_is_function, r) == false);
    assert(__reflect(query_is_function_parameter, r) == false);
    assert(__reflect(query_is_class, r) == false);
    assert(__reflect(query_is_nonstatic_data_member, r) == true);
    assert(__reflect(query_is_nonstatic_member_function, r) == false);
    // assert(is_access_specifier(r) == false);
    assert(__reflect(query_is_enum, r) == false);
    assert(__reflect(query_is_enumerator, r) == false);

    assert(__reflect(query_is_static_data_member, r) == false);
    assert(__reflect(query_is_static_member_function, r) == false);
    // assert(is_mutable(r) == true);
  }

  // static int S1::x3;
  {
    constexpr auto r = reflexpr(S1::x3);
    // assert(kind(r) == meta::data_member_decl);

    assert(__reflect(query_is_null_pointer, r) == false);
    // assert(is_declaration(r) == true);
    assert(__reflect(query_is_type, r) == false);
    // assert(is_translation_unit(r) == false);
    assert(__reflect(query_is_namespace, r) == false);
    assert(__reflect(query_is_variable, r) == false);
    assert(__reflect(query_is_function, r) == false);
    assert(__reflect(query_is_function_parameter, r) == false);
    assert(__reflect(query_is_class, r) == false);
    assert(__reflect(query_is_nonstatic_data_member, r) == true);
    assert(__reflect(query_is_nonstatic_member_function, r) == false);
    // assert(is_access_specifier(r) == false);
    assert(__reflect(query_is_enum, r) == false);
    assert(__reflect(query_is_enumerator, r) == false);

    assert(__reflect(query_is_static_data_member, r) == true);
    assert(__reflect(query_is_static_member_function, r) == false);
    // assert(is_mutable(r) == false);
  }

  // static constexpr int S1::x4;
  {
    constexpr auto r = reflexpr(S1::x4);
    // assert(kind(r) == meta::data_member_decl);

    assert(__reflect(query_is_null_pointer, r) == false);
    // assert(is_declaration(r) == true);
    assert(__reflect(query_is_type, r) == false);
    // assert(is_translation_unit(r) == false);
    assert(__reflect(query_is_namespace, r) == false);
    assert(__reflect(query_is_variable, r) == false);
    assert(__reflect(query_is_function, r) == false);
    assert(__reflect(query_is_function_parameter, r) == false);
    assert(__reflect(query_is_class, r) == false);
    assert(__reflect(query_is_nonstatic_data_member, r) == true);
    assert(__reflect(query_is_nonstatic_member_function, r) == false);
    // assert(is_access_specifier(r) == false);
    assert(__reflect(query_is_enum, r) == false);
    assert(__reflect(query_is_enumerator, r) == false);

    assert(__reflect(query_is_static_data_member, r) == true);
    assert(__reflect(query_is_static_member_function, r) == false);
    // assert(is_inline(r) == true);
    // assert(is_constexpr(r) == true);
  }

  // int S1::x5 : 4;
  {
    constexpr auto r = reflexpr(S1::x5);
    // assert(kind(r) == meta::data_member_decl);

    assert(__reflect(query_is_null_pointer, r) == false);
    // assert(is_declaration(r) == true);
    assert(__reflect(query_is_type, r) == false);
    // assert(is_translation_unit(r) == false);
    assert(__reflect(query_is_namespace, r) == false);
    assert(__reflect(query_is_variable, r) == false);
    assert(__reflect(query_is_function, r) == false);
    assert(__reflect(query_is_function_parameter, r) == false);
    assert(__reflect(query_is_class, r) == false);
    assert(__reflect(query_is_nonstatic_data_member, r) == true);
    assert(__reflect(query_is_nonstatic_member_function, r) == false);
    // assert(is_access_specifier(r) == false);
    assert(__reflect(query_is_enum, r) == false);
    assert(__reflect(query_is_enumerator, r) == false);

    assert(__reflect(query_is_static_data_member, r) == false);
    assert(__reflect(query_is_static_member_function, r) == false);
    assert(__reflect(query_is_bitfield, r) == true);
  }

  // Member functions

  // void S1::f1() { }
  {
    constexpr auto r = reflexpr(S1::f1);
    // assert(kind(r) == meta::member_function_decl);

    assert(__reflect(query_is_null_pointer, r) == false);
    // assert(is_declaration(r) == true);
    assert(__reflect(query_is_type, r) == false);
    // assert(is_translation_unit(r) == false);
    assert(__reflect(query_is_namespace, r) == false);
    assert(__reflect(query_is_variable, r) == false);
    assert(__reflect(query_is_function, r) == false);
    assert(__reflect(query_is_function_parameter, r) == false);
    assert(__reflect(query_is_class, r) == false);
    assert(__reflect(query_is_nonstatic_data_member, r) == false);
    assert(__reflect(query_is_nonstatic_member_function, r) == true);
    // assert(is_access_specifier(r) == false);
    assert(__reflect(query_is_enum, r) == false);
    assert(__reflect(query_is_enumerator, r) == false);

    // assert(is_public(r) == true);
    // assert(is_normal(r) == true);
    assert(__reflect(query_is_static_data_member, r) == false);
    assert(__reflect(query_is_static_member_function, r) == false);
    // assert(is_constexpr(r) == false);
    // assert(is_virtual(r) == false);
    // assert(is_pure_virtual(r) == false);
    // assert(is_override(r) == false);
    // assert(is_final(r) == false);
    // assert(is_defined(r) == true);
    // assert(is_inline(r) == true); // in-class members are implicitly inline
    // assert(is_deleted(r) == false);
  }

  // constexpr int S1::f2() const { }
  {
    constexpr auto r = reflexpr(S1::f2);
    // assert(kind(r) == meta::member_function_decl);

    assert(__reflect(query_is_null_pointer, r) == false);
    // assert(is_declaration(r) == true);
    assert(__reflect(query_is_type, r) == false);
    // assert(is_translation_unit(r) == false);
    assert(__reflect(query_is_namespace, r) == false);
    assert(__reflect(query_is_variable, r) == false);
    assert(__reflect(query_is_function, r) == false);
    assert(__reflect(query_is_function_parameter, r) == false);
    assert(__reflect(query_is_class, r) == false);
    assert(__reflect(query_is_nonstatic_data_member, r) == false);
    assert(__reflect(query_is_nonstatic_member_function, r) == true);
    // assert(is_access_specifier(r) == false);
    assert(__reflect(query_is_enum, r) == false);
    assert(__reflect(query_is_enumerator, r) == false);

    // assert(is_public(r) == true);
    // assert(is_normal(r) == true);
    assert(__reflect(query_is_static_data_member, r) == false);
    assert(__reflect(query_is_static_member_function, r) == false);
    // assert(is_constexpr(r) == true);
    // assert(is_virtual(r) == false);
    // assert(is_pure_virtual(r) == false);
    // assert(is_override(r) == false);
    // assert(is_final(r) == false);
    // assert(is_defined(r) == true);
    // assert(is_inline(r) == true); // in-class members are implicitly inline
    // assert(is_deleted(r) == false);
  }

  // static int S2::f1() const { }
  {
    constexpr auto r = reflexpr(S2::f1);
    // assert(kind(r) == meta::member_function_decl);

    assert(__reflect(query_is_null_pointer, r) == false);
    // assert(is_declaration(r) == true);
    assert(__reflect(query_is_type, r) == false);
    // assert(is_translation_unit(r) == false);
    assert(__reflect(query_is_namespace, r) == false);
    assert(__reflect(query_is_variable, r) == false);
    assert(__reflect(query_is_function, r) == false);
    assert(__reflect(query_is_function_parameter, r) == false);
    assert(__reflect(query_is_class, r) == false);
    assert(__reflect(query_is_nonstatic_data_member, r) == false);
    assert(__reflect(query_is_nonstatic_member_function, r) == true);
    // assert(is_access_specifier(r) == false);
    assert(__reflect(query_is_enum, r) == false);
    assert(__reflect(query_is_enumerator, r) == false);

    // assert(is_public(r) == true);
    // assert(is_normal(r) == true);
    assert(__reflect(query_is_static_data_member, r) == false);
    assert(__reflect(query_is_static_member_function, r) == true);
    // assert(is_constexpr(r) == false);
    // assert(is_virtual(r) == false);
    // assert(is_pure_virtual(r) == false);
    // assert(is_override(r) == false);
    // assert(is_final(r) == false);
    // assert(is_defined(r) == true);
    // assert(is_inline(r) == true); // in-class members are implicitly inline
    // assert(is_deleted(r) == false);
  }

  // FIXME: Add more tests for member functions.

  // Enums

  // enum E1 { ... }
  {
    constexpr auto r = reflexpr(E1);
    // assert(kind(r) == meta::enum_decl);

    assert(__reflect(query_is_null_pointer, r) == false);
    // assert(is_declaration(r) == true);
    assert(__reflect(query_is_type, r) == false);
    // assert(is_translation_unit(r) == false);
    assert(__reflect(query_is_namespace, r) == false);
    assert(__reflect(query_is_variable, r) == false);
    assert(__reflect(query_is_function, r) == false);
    assert(__reflect(query_is_function_parameter, r) == false);
    assert(__reflect(query_is_class, r) == false);
    assert(__reflect(query_is_nonstatic_data_member, r) == false);
    assert(__reflect(query_is_nonstatic_member_function, r) == false);
    // assert(is_access_specifier(r) == false);
    assert(__reflect(query_is_enum, r) == true);
    assert(__reflect(query_is_enumerator, r) == false);

    // assert(has_external_linkage(r) == true);
    // assert(has_access(r) == false);
    assert(__reflect(query_is_scoped_enum, r) == false);
  }

  // enum class E2 { ... }
  {
    constexpr auto r = reflexpr(E2);
    // assert(kind(r) == meta::enum_decl);

    assert(__reflect(query_is_null_pointer, r) == false);
    // assert(is_declaration(r) == true);
    assert(__reflect(query_is_type, r) == false);
    // assert(is_translation_unit(r) == false);
    assert(__reflect(query_is_namespace, r) == false);
    assert(__reflect(query_is_variable, r) == false);
    assert(__reflect(query_is_function, r) == false);
    assert(__reflect(query_is_function_parameter, r) == false);
    assert(__reflect(query_is_class, r) == false);
    assert(__reflect(query_is_nonstatic_data_member, r) == false);
    assert(__reflect(query_is_nonstatic_member_function, r) == false);
    // assert(is_access_specifier(r) == false);
    assert(__reflect(query_is_enum, r) == true);
    assert(__reflect(query_is_enumerator, r) == false);

    // assert(has_external_linkage(r) == true);
    // assert(has_access(r) == false);
    assert(__reflect(query_is_scoped_enum, r) == true);
  }

  // enum class : int;
  {
    constexpr auto r = reflexpr(E3);
    // assert(kind(r) == meta::enum_decl);

    assert(__reflect(query_is_null_pointer, r) == false);
    // assert(is_declaration(r) == true);
    assert(__reflect(query_is_type, r) == false);
    // assert(is_translation_unit(r) == false);
    assert(__reflect(query_is_namespace, r) == false);
    assert(__reflect(query_is_variable, r) == false);
    assert(__reflect(query_is_function, r) == false);
    assert(__reflect(query_is_function_parameter, r) == false);
    assert(__reflect(query_is_class, r) == false);
    assert(__reflect(query_is_nonstatic_data_member, r) == false);
    assert(__reflect(query_is_nonstatic_member_function, r) == false);
    // assert(is_access_specifier(r) == false);
    assert(__reflect(query_is_enum, r) == true);
    assert(__reflect(query_is_enumerator, r) == false);

    // assert(has_external_linkage(r) == true);
    // assert(has_access(r) == false);
    assert(__reflect(query_is_scoped_enum, r) == true);
  }

  // Enumerators

  // enum E1 { X }
  {
    constexpr auto r = reflexpr(X);
    // assert(kind(r) == meta::enumerator_decl);

    assert(__reflect(query_is_null_pointer, r) == false);
    // assert(is_declaration(r) == true);
    assert(__reflect(query_is_type, r) == false);
    // assert(is_translation_unit(r) == false);
    assert(__reflect(query_is_namespace, r) == false);
    assert(__reflect(query_is_variable, r) == false);
    assert(__reflect(query_is_function, r) == false);
    assert(__reflect(query_is_function_parameter, r) == false);
    assert(__reflect(query_is_class, r) == false);
    assert(__reflect(query_is_nonstatic_data_member, r) == false);
    assert(__reflect(query_is_nonstatic_member_function, r) == false);
    // assert(is_access_specifier(r) == false);
    assert(__reflect(query_is_enum, r) == false);
    assert(__reflect(query_is_enumerator, r) == true);

    // assert(has_access(r) == false);
  }

  // enum class E2 { X }
  {
    constexpr auto r = reflexpr(E2::X);
    // assert(kind(r) == meta::enumerator_decl);

    assert(__reflect(query_is_null_pointer, r) == false);
    // assert(is_declaration(r) == true);
    assert(__reflect(query_is_type, r) == false);
    // assert(is_translation_unit(r) == false);
    assert(__reflect(query_is_namespace, r) == false);
    assert(__reflect(query_is_variable, r) == false);
    assert(__reflect(query_is_function, r) == false);
    assert(__reflect(query_is_function_parameter, r) == false);
    assert(__reflect(query_is_class, r) == false);
    assert(__reflect(query_is_nonstatic_data_member, r) == false);
    assert(__reflect(query_is_nonstatic_member_function, r) == false);
    // assert(is_access_specifier(r) == false);
    assert(__reflect(query_is_enum, r) == false);
    assert(__reflect(query_is_enumerator, r) == true);

    // assert(has_access(r) == false);
  }

}
