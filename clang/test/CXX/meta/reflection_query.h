#ifndef REFLECTION_QUERY_H
#define REFLECTION_QUERY_H

namespace meta {
  using info = decltype(reflexpr(void));
}

enum reflection_query {
  query_unknown,

  query_is_invalid,
  query_is_entity,
  query_is_named,

  // Scopes
  query_is_local,

  // Variables
  query_is_variable,
  query_has_static_storage,
  query_has_thread_local_storage,
  query_has_automatic_storage,

  // Functions
  query_is_function,
  query_is_nothrow,
  // query_has_ellipsis,

  // Classes
  query_is_class,
  query_is_union,
  query_has_virtual_destructor,
  query_is_declared_struct,
  query_is_declared_class,

  // Class members
  query_is_class_member,

  // Data members
  query_is_static_data_member,
  query_is_nonstatic_data_member,
  query_is_bit_field,
  query_is_mutable,

  // Member functions
  query_is_static_member_function,
  query_is_nonstatic_member_function,
  query_is_normal,
  query_is_override,
  query_is_override_specified,
  query_is_deleted,
  query_is_virtual,
  query_is_pure_virtual,

  // Special members
  query_is_constructor,
  query_is_default_constructor,
  query_is_copy_constructor,
  query_is_move_constructor,
  query_is_copy_assignment_operator,
  query_is_move_assignment_operator,
  query_is_destructor,
  query_is_conversion,
  query_is_defaulted,
  query_is_explicit,

  // Access
  query_has_access,
  query_is_public,
  query_is_protected,
  query_is_private,
  query_has_default_access,

  // Linkage
  query_has_linkage,
  query_is_externally_linked,
  query_is_internally_linked,

  // Initializers
  query_has_initializer,

  // General purpose
  query_is_extern_specified,
  query_is_inline,
  query_is_inline_specified,
  query_is_constexpr,
  query_is_consteval,
  query_is_final,
  query_is_defined,
  query_is_complete,

  // Namespaces
  query_is_namespace,

  // Aliases
  query_is_namespace_alias,
  query_is_type_alias,
  query_is_alias_template,

  // Enums
  query_is_unscoped_enum,
  query_is_scoped_enum,

  // Enumerators
  query_is_enumerator,

  // Templates
  query_is_template,
  query_is_class_template,
  query_is_function_template,
  query_is_variable_template,
  query_is_static_member_function_template,
  query_is_nonstatic_member_function_template,
  query_is_constructor_template,
  query_is_destructor_template,
  query_is_concept,

  // Specializations
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
  query_is_type_template_parameter,
  query_is_nontype_template_parameter,
  query_is_template_template_parameter,
  query_has_default_argument,

  // Attributes
  query_has_attribute,

  // Types
  query_is_type,
  query_is_fundamental_type,
  query_is_arithmetic_type,
  query_is_scalar_type,
  query_is_object_type,
  query_is_compound_type,
  query_is_function_type,
  query_is_class_type,
  query_is_union_type,
  query_is_unscoped_enum_type,
  query_is_scoped_enum_type,
  query_is_void_type,
  query_is_null_pointer_type,
  query_is_integral_type,
  query_is_floating_point_type,
  query_is_array_type,
  query_is_pointer_type,
  query_is_lvalue_reference_type,
  query_is_rvalue_reference_type,
  query_is_member_object_pointer_type,
  query_is_member_function_pointer_type,
  query_is_closure_type,

  // Type properties
  query_is_incomplete_type,
  query_is_const_type,
  query_is_volatile_type,
  query_is_trivial_type,
  query_is_trivially_copyable_type,
  query_is_standard_layout_type,
  query_is_pod_type,
  query_is_literal_type,
  query_is_empty_type,
  query_is_polymorphic_type,
  query_is_abstract_type,
  query_is_final_type,
  query_is_aggregate_type,
  query_is_signed_type,
  query_is_unsigned_type,
  query_has_unique_object_representations_type,

  // Type operations
  query_is_constructible,
  query_is_trivially_constructible,
  query_is_nothrow_constructible,
  query_is_assignable,
  query_is_trivially_assignable,
  query_is_nothrow_assignable,
  query_is_destructible,
  query_is_trivially_destructible,
  query_is_nothrow_destructible,

  // Captures
  query_has_default_ref_capture,
  query_has_default_copy_capture,
  query_is_capture,
  query_is_simple_capture,
  query_is_ref_capture,
  query_is_copy_capture,
  query_is_explicit_capture,
  query_is_init_capture,
  query_has_captures,

  // Expressions
  query_is_expression,
  query_is_lvalue,
  query_is_xvalue,
  query_is_prvalue,
  query_is_value,

  // Associated types
  query_get_type,
  query_get_return_type,
  query_get_this_ref_type,
  query_get_underlying_type,

  // Entities
  query_get_entity,
  query_get_parent,
  query_get_definition,

  // Traversal
  query_get_begin,
  query_get_next,
  query_get_begin_template_param,
  query_get_next_template_param,
  query_get_begin_param,
  query_get_next_param,
  query_get_begin_member,
  query_get_next_member,
  query_get_begin_base_spec,
  query_get_next_base_spec,

  // Type transformations
  query_remove_const,
  query_remove_volatile,
  query_add_const,
  query_add_volatile,
  query_remove_reference,
  query_add_lvalue_reference,
  query_add_rvalue_reference,
  query_remove_extent,
  query_remove_pointer,
  query_add_pointer,
  query_make_signed,
  query_make_unsigned,

  // Names
  query_get_name,
  query_get_display_name,

  // Modifier updates
  query_set_access,
  query_set_storage,
  query_set_constexpr,
  query_set_add_explicit,
  query_set_add_virtual,
  query_set_add_pure_virtual,
  query_set_add_inline,
  query_set_new_name
};

#endif
