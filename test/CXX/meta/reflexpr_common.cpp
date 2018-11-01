// RUN: %clang_cc1 -freflection -std=c++1z %s

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
  constexpr meta::info r = reflexpr(y1);
  static_assert(kind(r) == meta::data_member_decl);

  static_assert(is_null(r) == false);
  static_assert(is_declaration(r) == true);
  static_assert(is_type(r) == false);
  static_assert(is_translation_unit(r) == false);
  static_assert(is_namespace(r) == false);
  static_assert(is_variable(r) == false);
  static_assert(is_function(r) == false);
  static_assert(is_parameter(r) == false);
  static_assert(is_class(r) == false);
  static_assert(is_data_member(r) == true);
  static_assert(is_member_function(r) == false);
  static_assert(is_access_specifier(r) == false);
  static_assert(is_enum(r) == false);
  static_assert(is_enumerator(r) == false);

  static_assert(is_public(r) == false);
  static_assert(is_static(r) == false);
  static_assert(is_mutable(r) == false);
  static_assert(is_inline(r) == false);
  static_assert(is_constexpr(r) == false);
  static_assert(is_bitfield(r) == false);

}

int main(int argc, char* argv[]) {

  // int x1 -- global
  {
    constexpr meta::info r = reflexpr(x1);
    static_assert(kind(r) == meta::variable_decl);

    static_assert(is_null(r) == false);
    static_assert(is_declaration(r) == true);
    static_assert(is_type(r) == false);
    static_assert(is_translation_unit(r) == false);
    static_assert(is_namespace(r) == false);
    static_assert(is_variable(r) == true);
    static_assert(is_function(r) == false);
    static_assert(is_parameter(r) == false);
    static_assert(is_class(r) == false);
    static_assert(is_data_member(r) == false);
    static_assert(is_member_function(r) == false);
    static_assert(is_access_specifier(r) == false);
    static_assert(is_enum(r) == false);
    static_assert(is_enumerator(r) == false);

    static_assert(has_external_linkage(r) == true);
    static_assert(has_static_storage(r) == true);
    static_assert(is_extern(r) == false);
    static_assert(is_static(r) == false);
    static_assert(is_inline(r) == false);
    static_assert(is_constexpr(r) == false);
  }

  // static int x2 -- global
  {
    constexpr meta::info r = reflexpr(x2);
    static_assert(kind(r) == meta::variable_decl);

    static_assert(is_null(r) == false);
    static_assert(is_declaration(r) == true);
    static_assert(is_type(r) == false);
    static_assert(is_translation_unit(r) == false);
    static_assert(is_namespace(r) == false);
    static_assert(is_variable(r) == true);
    static_assert(is_function(r) == false);
    static_assert(is_parameter(r) == false);
    static_assert(is_class(r) == false);
    static_assert(is_data_member(r) == false);
    static_assert(is_member_function(r) == false);
    static_assert(is_access_specifier(r) == false);
    static_assert(is_enum(r) == false);
    static_assert(is_enumerator(r) == false);

    static_assert(has_internal_linkage(r) == true);
    static_assert(has_static_storage(r) == true);
    static_assert(is_extern(r) == false);
    static_assert(is_static(r) == true);
    static_assert(is_inline(r) == false);
    static_assert(is_constexpr(r) == false);
  }

  // Functions
  
  // void f1() { }
  {
    constexpr meta::info r = reflexpr(f1);
    static_assert(kind(r) == meta::function_decl);

    static_assert(is_null(r) == false);
    static_assert(is_declaration(r) == true);
    static_assert(is_type(r) == false);
    static_assert(is_translation_unit(r) == false);
    static_assert(is_namespace(r) == false);
    static_assert(is_variable(r) == false);
    static_assert(is_function(r) == true);
    static_assert(is_parameter(r) == false);
    static_assert(is_class(r) == false);
    static_assert(is_data_member(r) == false);
    static_assert(is_member_function(r) == false);
    static_assert(is_access_specifier(r) == false);
    static_assert(is_enum(r) == false);
    static_assert(is_enumerator(r) == false);

    static_assert(has_external_linkage(r) == true);
    static_assert(is_static(r) == false);
    static_assert(is_extern(r) == false);
    static_assert(is_constexpr(r) == false);
    static_assert(is_defined(r) == true);
    static_assert(is_inline(r) == false);
    static_assert(is_deleted(r) == false);
  }

  // void f2();
  {
    constexpr meta::info r = reflexpr(f2);
    static_assert(kind(r) == meta::function_decl);

    static_assert(is_null(r) == false);
    static_assert(is_declaration(r) == true);
    static_assert(is_type(r) == false);
    static_assert(is_translation_unit(r) == false);
    static_assert(is_namespace(r) == false);
    static_assert(is_variable(r) == false);
    static_assert(is_function(r) == true);
    static_assert(is_parameter(r) == false);
    static_assert(is_class(r) == false);
    static_assert(is_data_member(r) == false);
    static_assert(is_member_function(r) == false);
    static_assert(is_access_specifier(r) == false);
    static_assert(is_enum(r) == false);
    static_assert(is_enumerator(r) == false);

    static_assert(has_external_linkage(r) == true);
    static_assert(is_static(r) == false);
    static_assert(is_extern(r) == false);
    static_assert(is_constexpr(r) == false);
    static_assert(is_defined(r) == false);
  }

  // static inline void f3() { }
  {
    constexpr meta::info r = reflexpr(f3);
    static_assert(kind(r) == meta::function_decl);

    static_assert(is_null(r) == false);
    static_assert(is_declaration(r) == true);
    static_assert(is_type(r) == false);
    static_assert(is_translation_unit(r) == false);
    static_assert(is_namespace(r) == false);
    static_assert(is_variable(r) == false);
    static_assert(is_function(r) == true);
    static_assert(is_parameter(r) == false);
    static_assert(is_class(r) == false);
    static_assert(is_data_member(r) == false);
    static_assert(is_member_function(r) == false);
    static_assert(is_access_specifier(r) == false);
    static_assert(is_enum(r) == false);
    static_assert(is_enumerator(r) == false);

    static_assert(has_internal_linkage(r) == true);
    static_assert(is_static(r) == true);
    static_assert(is_extern(r) == false);
    static_assert(is_constexpr(r) == false);
    static_assert(is_defined(r) == true);
    static_assert(is_inline(r) == true);
    static_assert(is_deleted(r) == false);
  }

  // constexpr int f4() { return 0; }
  {
    constexpr meta::info r = reflexpr(f4);
    static_assert(kind(r) == meta::function_decl);

    static_assert(is_null(r) == false);
    static_assert(is_declaration(r) == true);
    static_assert(is_type(r) == false);
    static_assert(is_translation_unit(r) == false);
    static_assert(is_namespace(r) == false);
    static_assert(is_variable(r) == false);
    static_assert(is_function(r) == true);
    static_assert(is_parameter(r) == false);
    static_assert(is_class(r) == false);
    static_assert(is_data_member(r) == false);
    static_assert(is_member_function(r) == false);
    static_assert(is_access_specifier(r) == false);
    static_assert(is_enum(r) == false);
    static_assert(is_enumerator(r) == false);

    static_assert(has_external_linkage(r) == true);
    static_assert(is_static(r) == false);
    static_assert(is_extern(r) == false);
    static_assert(is_constexpr(r) == true);
    static_assert(is_defined(r) == true);
    static_assert(is_inline(r) == true);
    static_assert(is_deleted(r) == false);
  }

  // void f5(int) = delete;
  {
    constexpr meta::info r = reflexpr(f5);
    static_assert(kind(r) == meta::function_decl);

    static_assert(is_null(r) == false);
    static_assert(is_declaration(r) == true);
    static_assert(is_type(r) == false);
    static_assert(is_translation_unit(r) == false);
    static_assert(is_namespace(r) == false);
    static_assert(is_variable(r) == false);
    static_assert(is_function(r) == true);
    static_assert(is_parameter(r) == false);
    static_assert(is_class(r) == false);
    static_assert(is_data_member(r) == false);
    static_assert(is_member_function(r) == false);
    static_assert(is_access_specifier(r) == false);
    static_assert(is_enum(r) == false);
    static_assert(is_enumerator(r) == false);

    static_assert(has_external_linkage(r) == true);
    static_assert(is_static(r) == false);
    static_assert(is_extern(r) == false);
    static_assert(is_constexpr(r) == false);
    static_assert(is_defined(r) == true);
    static_assert(is_inline(r) == true); // FIXME: Apparently so.
    static_assert(is_deleted(r) == true);
  }

  // Namespaces
  
  // namespace N
  {
    constexpr meta::info r = reflexpr(N);
    static_assert(kind(r) == meta::namespace_decl);

    static_assert(is_null(r) == false);
    static_assert(is_declaration(r) == true);
    static_assert(is_type(r) == false);
    static_assert(is_translation_unit(r) == false);
    static_assert(is_namespace(r) == true);
    static_assert(is_variable(r) == false);
    static_assert(is_function(r) == false);
    static_assert(is_parameter(r) == false);
    static_assert(is_class(r) == false);
    static_assert(is_data_member(r) == false);
    static_assert(is_member_function(r) == false);
    static_assert(is_access_specifier(r) == false);
    static_assert(is_enum(r) == false);
    static_assert(is_enumerator(r) == false);

    static_assert(is_inline(r) == false);
  }

  // inline namespace N::M
  {
    constexpr meta::info r = reflexpr(N::M);
    static_assert(kind(r) == meta::namespace_decl); 

    static_assert(is_null(r) == false);
    static_assert(is_declaration(r) == true);
    static_assert(is_type(r) == false);
    static_assert(is_translation_unit(r) == false);
    static_assert(is_namespace(r) == true);
    static_assert(is_variable(r) == false);
    static_assert(is_function(r) == false);
    static_assert(is_parameter(r) == false);
    static_assert(is_class(r) == false);
    static_assert(is_data_member(r) == false);
    static_assert(is_member_function(r) == false);
    static_assert(is_access_specifier(r) == false);
    static_assert(is_enum(r) == false);
    static_assert(is_enumerator(r) == false);

    static_assert(is_inline(r) == true);
  }

  // Classes

  // class C1 { };
  {
    constexpr meta::info r = reflexpr(C1);
    static_assert(kind(r) == meta::class_decl);

    static_assert(is_null(r) == false);
    static_assert(is_declaration(r) == true);
    static_assert(is_type(r) == false);
    static_assert(is_translation_unit(r) == false);
    static_assert(is_namespace(r) == false);
    static_assert(is_variable(r) == false);
    static_assert(is_function(r) == false);
    static_assert(is_parameter(r) == false);
    static_assert(is_class(r) == true);
    static_assert(is_data_member(r) == false);
    static_assert(is_member_function(r) == false);
    static_assert(is_access_specifier(r) == false);
    static_assert(is_enum(r) == false);
    static_assert(is_enumerator(r) == false);

    static_assert(has_external_linkage(r) == true);
    static_assert(has_access(r) == false);
    static_assert(is_class(r) == true);
    static_assert(is_declared_class(r) == true);
    static_assert(is_complete(r) == true);
  }

  // class C2;
  {
    constexpr meta::info r = reflexpr(C2);
    static_assert(kind(r) == meta::class_decl);

    static_assert(is_null(r) == false);
    static_assert(is_declaration(r) == true);
    static_assert(is_type(r) == false);
    static_assert(is_translation_unit(r) == false);
    static_assert(is_namespace(r) == false);
    static_assert(is_variable(r) == false);
    static_assert(is_function(r) == false);
    static_assert(is_parameter(r) == false);
    static_assert(is_class(r) == true);
    static_assert(is_data_member(r) == false);
    static_assert(is_member_function(r) == false);
    static_assert(is_access_specifier(r) == false);
    static_assert(is_enum(r) == false);
    static_assert(is_enumerator(r) == false);

    static_assert(has_external_linkage(r) == true);
    static_assert(has_access(r) == false);
    static_assert(is_class(r) == true);
    static_assert(is_complete(r) == false);
  }

  // struct S1 { ... };
  {
    constexpr meta::info r = reflexpr(S1);
    static_assert(kind(r) == meta::class_decl);

    static_assert(is_null(r) == false);
    static_assert(is_declaration(r) == true);
    static_assert(is_type(r) == false);
    static_assert(is_translation_unit(r) == false);
    static_assert(is_namespace(r) == false);
    static_assert(is_variable(r) == false);
    static_assert(is_function(r) == false);
    static_assert(is_parameter(r) == false);
    static_assert(is_class(r) == true);
    static_assert(is_data_member(r) == false);
    static_assert(is_member_function(r) == false);
    static_assert(is_access_specifier(r) == false);
    static_assert(is_enum(r) == false);
    static_assert(is_enumerator(r) == false);

    static_assert(is_declared_struct(r) == true);
  }

  // class S1::C { ... };
  {
    constexpr meta::info r = reflexpr(S1::C);
    static_assert(kind(r) == meta::class_decl);

    static_assert(is_null(r) == false);
    static_assert(is_declaration(r) == true);
    static_assert(is_type(r) == false);
    static_assert(is_translation_unit(r) == false);
    static_assert(is_namespace(r) == false);
    static_assert(is_variable(r) == false);
    static_assert(is_function(r) == false);
    static_assert(is_parameter(r) == false);
    static_assert(is_class(r) == true);
    static_assert(is_data_member(r) == false);
    static_assert(is_member_function(r) == false);
    static_assert(is_access_specifier(r) == false);
    static_assert(is_enum(r) == false);
    static_assert(is_enumerator(r) == false);

    static_assert(has_access(r) == true);
    static_assert(is_public(r) == true);
  }
  
  // union U1 { ... };
  {
    constexpr meta::info r = reflexpr(U1);
    static_assert(kind(r) == meta::class_decl);

    static_assert(is_null(r) == false);
    static_assert(is_declaration(r) == true);
    static_assert(is_type(r) == false);
    static_assert(is_translation_unit(r) == false);
    static_assert(is_namespace(r) == false);
    static_assert(is_variable(r) == false);
    static_assert(is_function(r) == false);
    static_assert(is_parameter(r) == false);
    static_assert(is_class(r) == true);
    static_assert(is_data_member(r) == false);
    static_assert(is_member_function(r) == false);
    static_assert(is_access_specifier(r) == false);
    static_assert(is_enum(r) == false);
    static_assert(is_enumerator(r) == false);

    static_assert(is_union(r) == true);
  }

  // Data members

  // int S1::x1;
  {
    constexpr meta::info r = reflexpr(S1::x1);
    static_assert(kind(r) == meta::data_member_decl);

    static_assert(is_null(r) == false);
    static_assert(is_declaration(r) == true);
    static_assert(is_type(r) == false);
    static_assert(is_translation_unit(r) == false);
    static_assert(is_namespace(r) == false);
    static_assert(is_variable(r) == false);
    static_assert(is_function(r) == false);
    static_assert(is_parameter(r) == false);
    static_assert(is_class(r) == false);
    static_assert(is_data_member(r) == true);
    static_assert(is_member_function(r) == false);
    static_assert(is_access_specifier(r) == false);
    static_assert(is_enum(r) == false);
    static_assert(is_enumerator(r) == false);

    static_assert(is_public(r) == true);
    static_assert(is_static(r) == false);
    static_assert(is_mutable(r) == false);
    static_assert(is_inline(r) == false);
    static_assert(is_constexpr(r) == false);
    static_assert(is_bitfield(r) == false);
  }

  // mutable int S1::x2;
  {
    constexpr meta::info r = reflexpr(S1::x2);
    static_assert(kind(r) == meta::data_member_decl);

    static_assert(is_null(r) == false);
    static_assert(is_declaration(r) == true);
    static_assert(is_type(r) == false);
    static_assert(is_translation_unit(r) == false);
    static_assert(is_namespace(r) == false);
    static_assert(is_variable(r) == false);
    static_assert(is_function(r) == false);
    static_assert(is_parameter(r) == false);
    static_assert(is_class(r) == false);
    static_assert(is_data_member(r) == true);
    static_assert(is_member_function(r) == false);
    static_assert(is_access_specifier(r) == false);
    static_assert(is_enum(r) == false);
    static_assert(is_enumerator(r) == false);

    static_assert(is_static(r) == false);
    static_assert(is_mutable(r) == true);
  }

  // static int S1::x3;
  {
    constexpr meta::info r = reflexpr(S1::x3);
    static_assert(kind(r) == meta::data_member_decl);

    static_assert(is_null(r) == false);
    static_assert(is_declaration(r) == true);
    static_assert(is_type(r) == false);
    static_assert(is_translation_unit(r) == false);
    static_assert(is_namespace(r) == false);
    static_assert(is_variable(r) == false);
    static_assert(is_function(r) == false);
    static_assert(is_parameter(r) == false);
    static_assert(is_class(r) == false);
    static_assert(is_data_member(r) == true);
    static_assert(is_member_function(r) == false);
    static_assert(is_access_specifier(r) == false);
    static_assert(is_enum(r) == false);
    static_assert(is_enumerator(r) == false);

    static_assert(is_static(r) == true);
    static_assert(is_mutable(r) == false);
  }

  // static constexpr int S1::x4;
  {
    constexpr meta::info r = reflexpr(S1::x4);
    static_assert(kind(r) == meta::data_member_decl);

    static_assert(is_null(r) == false);
    static_assert(is_declaration(r) == true);
    static_assert(is_type(r) == false);
    static_assert(is_translation_unit(r) == false);
    static_assert(is_namespace(r) == false);
    static_assert(is_variable(r) == false);
    static_assert(is_function(r) == false);
    static_assert(is_parameter(r) == false);
    static_assert(is_class(r) == false);
    static_assert(is_data_member(r) == true);
    static_assert(is_member_function(r) == false);
    static_assert(is_access_specifier(r) == false);
    static_assert(is_enum(r) == false);
    static_assert(is_enumerator(r) == false);

    static_assert(is_static(r) == true);
    static_assert(is_inline(r) == true);
    static_assert(is_constexpr(r) == true);
  }

  // int S1::x5 : 4;
  {
    constexpr meta::info r = reflexpr(S1::x5);
    static_assert(kind(r) == meta::data_member_decl);

    static_assert(is_null(r) == false);
    static_assert(is_declaration(r) == true);
    static_assert(is_type(r) == false);
    static_assert(is_translation_unit(r) == false);
    static_assert(is_namespace(r) == false);
    static_assert(is_variable(r) == false);
    static_assert(is_function(r) == false);
    static_assert(is_parameter(r) == false);
    static_assert(is_class(r) == false);
    static_assert(is_data_member(r) == true);
    static_assert(is_member_function(r) == false);
    static_assert(is_access_specifier(r) == false);
    static_assert(is_enum(r) == false);
    static_assert(is_enumerator(r) == false);

    static_assert(is_static(r) == false);
    static_assert(is_bitfield(r) == true);
  }

  // Member functions

  // void S1::f1() { }
  {
    constexpr meta::info r = reflexpr(S1::f1);
    static_assert(kind(r) == meta::member_function_decl);

    static_assert(is_null(r) == false);
    static_assert(is_declaration(r) == true);
    static_assert(is_type(r) == false);
    static_assert(is_translation_unit(r) == false);
    static_assert(is_namespace(r) == false);
    static_assert(is_variable(r) == false);
    static_assert(is_function(r) == false);
    static_assert(is_parameter(r) == false);
    static_assert(is_class(r) == false);
    static_assert(is_data_member(r) == false);
    static_assert(is_member_function(r) == true);
    static_assert(is_access_specifier(r) == false);
    static_assert(is_enum(r) == false);
    static_assert(is_enumerator(r) == false);
    
    static_assert(is_public(r) == true);
    static_assert(is_normal(r) == true);
    static_assert(is_static(r) == false);
    static_assert(is_constexpr(r) == false);
    static_assert(is_virtual(r) == false);
    static_assert(is_pure_virtual(r) == false);
    static_assert(is_override(r) == false);
    static_assert(is_final(r) == false);
    static_assert(is_defined(r) == true);
    static_assert(is_inline(r) == true); // in-class members are implicitly inline
    static_assert(is_deleted(r) == false);
  }

  // constexpr int S1::f2() const { }
  {
    constexpr meta::info r = reflexpr(S1::f2);
    static_assert(kind(r) == meta::member_function_decl);

    static_assert(is_null(r) == false);
    static_assert(is_declaration(r) == true);
    static_assert(is_type(r) == false);
    static_assert(is_translation_unit(r) == false);
    static_assert(is_namespace(r) == false);
    static_assert(is_variable(r) == false);
    static_assert(is_function(r) == false);
    static_assert(is_parameter(r) == false);
    static_assert(is_class(r) == false);
    static_assert(is_data_member(r) == false);
    static_assert(is_member_function(r) == true);
    static_assert(is_access_specifier(r) == false);
    static_assert(is_enum(r) == false);
    static_assert(is_enumerator(r) == false);

    static_assert(is_public(r) == true);
    static_assert(is_normal(r) == true);
    static_assert(is_static(r) == false);
    static_assert(is_constexpr(r) == true);
    static_assert(is_virtual(r) == false);
    static_assert(is_pure_virtual(r) == false);
    static_assert(is_override(r) == false);
    static_assert(is_final(r) == false);
    static_assert(is_defined(r) == true);
    static_assert(is_inline(r) == true); // in-class members are implicitly inline
    static_assert(is_deleted(r) == false);
  }

  // static int S2::f1() const { }
  {
    constexpr meta::info r = reflexpr(S2::f1);
    static_assert(kind(r) == meta::member_function_decl);

    static_assert(is_null(r) == false);
    static_assert(is_declaration(r) == true);
    static_assert(is_type(r) == false);
    static_assert(is_translation_unit(r) == false);
    static_assert(is_namespace(r) == false);
    static_assert(is_variable(r) == false);
    static_assert(is_function(r) == false);
    static_assert(is_parameter(r) == false);
    static_assert(is_class(r) == false);
    static_assert(is_data_member(r) == false);
    static_assert(is_member_function(r) == true);
    static_assert(is_access_specifier(r) == false);
    static_assert(is_enum(r) == false);
    static_assert(is_enumerator(r) == false);

    static_assert(is_public(r) == true);
    static_assert(is_normal(r) == true);
    static_assert(is_static(r) == true);
    static_assert(is_constexpr(r) == false);
    static_assert(is_virtual(r) == false);
    static_assert(is_pure_virtual(r) == false);
    static_assert(is_override(r) == false);
    static_assert(is_final(r) == false);
    static_assert(is_defined(r) == true);
    static_assert(is_inline(r) == true); // in-class members are implicitly inline
    static_assert(is_deleted(r) == false);
  }

  // FIXME: Add more tests for member functions.

  // Enums

  // enum E1 { ... }
  {
    constexpr meta::info r = reflexpr(E1);
    static_assert(kind(r) == meta::enum_decl);

    static_assert(is_null(r) == false);
    static_assert(is_declaration(r) == true);
    static_assert(is_type(r) == false);
    static_assert(is_translation_unit(r) == false);
    static_assert(is_namespace(r) == false);
    static_assert(is_variable(r) == false);
    static_assert(is_function(r) == false);
    static_assert(is_parameter(r) == false);
    static_assert(is_class(r) == false);
    static_assert(is_data_member(r) == false);
    static_assert(is_member_function(r) == false);
    static_assert(is_access_specifier(r) == false);
    static_assert(is_enum(r) == true);
    static_assert(is_enumerator(r) == false);

    static_assert(has_external_linkage(r) == true);
    static_assert(has_access(r) == false);
    static_assert(is_scoped(r) == false);
  }

  // enum class E2 { ... }
  {
    constexpr meta::info r = reflexpr(E2);
    static_assert(kind(r) == meta::enum_decl);

    static_assert(is_null(r) == false);
    static_assert(is_declaration(r) == true);
    static_assert(is_type(r) == false);
    static_assert(is_translation_unit(r) == false);
    static_assert(is_namespace(r) == false);
    static_assert(is_variable(r) == false);
    static_assert(is_function(r) == false);
    static_assert(is_parameter(r) == false);
    static_assert(is_class(r) == false);
    static_assert(is_data_member(r) == false);
    static_assert(is_member_function(r) == false);
    static_assert(is_access_specifier(r) == false);
    static_assert(is_enum(r) == true);
    static_assert(is_enumerator(r) == false);

    static_assert(has_external_linkage(r) == true);
    static_assert(has_access(r) == false);
    static_assert(is_scoped(r) == true);
  }

  // enum class : int;
  {
    constexpr meta::info r = reflexpr(E3);
    static_assert(kind(r) == meta::enum_decl);

    static_assert(is_null(r) == false);
    static_assert(is_declaration(r) == true);
    static_assert(is_type(r) == false);
    static_assert(is_translation_unit(r) == false);
    static_assert(is_namespace(r) == false);
    static_assert(is_variable(r) == false);
    static_assert(is_function(r) == false);
    static_assert(is_parameter(r) == false);
    static_assert(is_class(r) == false);
    static_assert(is_data_member(r) == false);
    static_assert(is_member_function(r) == false);
    static_assert(is_access_specifier(r) == false);
    static_assert(is_enum(r) == true);
    static_assert(is_enumerator(r) == false);

    static_assert(has_external_linkage(r) == true);
    static_assert(has_access(r) == false);
    static_assert(is_scoped(r) == true);
  }

  // Enumerators

  // enum E1 { X }
  {
    constexpr meta::info r = reflexpr(X);
    static_assert(kind(r) == meta::enumerator_decl);

    static_assert(is_null(r) == false);
    static_assert(is_declaration(r) == true);
    static_assert(is_type(r) == false);
    static_assert(is_translation_unit(r) == false);
    static_assert(is_namespace(r) == false);
    static_assert(is_variable(r) == false);
    static_assert(is_function(r) == false);
    static_assert(is_parameter(r) == false);
    static_assert(is_class(r) == false);
    static_assert(is_data_member(r) == false);
    static_assert(is_member_function(r) == false);
    static_assert(is_access_specifier(r) == false);
    static_assert(is_enum(r) == false);
    static_assert(is_enumerator(r) == true);

    static_assert(has_access(r) == false);
  }

  // enum class E2 { X }
  {
    constexpr meta::info r = reflexpr(E2::X);
    static_assert(kind(r) == meta::enumerator_decl);

    static_assert(is_null(r) == false);
    static_assert(is_declaration(r) == true);
    static_assert(is_type(r) == false);
    static_assert(is_translation_unit(r) == false);
    static_assert(is_namespace(r) == false);
    static_assert(is_variable(r) == false);
    static_assert(is_function(r) == false);
    static_assert(is_parameter(r) == false);
    static_assert(is_class(r) == false);
    static_assert(is_data_member(r) == false);
    static_assert(is_member_function(r) == false);
    static_assert(is_access_specifier(r) == false);
    static_assert(is_enum(r) == false);
    static_assert(is_enumerator(r) == true);

    static_assert(has_access(r) == false);
  }

}
