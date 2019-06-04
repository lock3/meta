#ifndef REFLECTION_QUERY_H
#define REFLECTION_QUERY_H

namespace meta {
  using info = decltype(reflexpr(void));
}

enum reflection_query {
    query_unknown,

    query_is_invalid,
    query_is_entity,
    query_is_unnamed,

    /// Scope
    query_is_local,
    query_is_class_member,

    /// Declarations

    // Variables
    query_is_variable,
    query_has_static_storage,
    query_has_thread_local_storage,
    query_has_automatic_local_storage,

    // Functions
    query_is_function,
    // query_has_ellipsis,

    // Classes
    query_is_class,
    query_has_virtual_destructor,

    // Class Members

    // Data Members
    query_is_static_data_member,
    query_is_nonstatic_data_member,
    query_is_bit_field,

    // Member Functions
    query_is_static_member_function,
    query_is_nonstatic_member_function,
    query_is_override,
    query_is_override_specified,
    query_is_deleted,
    query_is_virtual,
    query_is_pure_virtual,

    // Special Members
    query_is_constructor,
    query_is_default_constructor,
    query_is_copy_constructor,
    query_is_move_constructor,
    query_is_copy_assignment_operator,
    query_is_move_assignment_operator,
    query_is_destructor,
    query_is_defaulted,
    query_is_explicit,

    // Access
    query_has_access,
    query_is_public,
    query_is_protected,
    query_is_private,
    query_has_default_access,

    // Union
    query_is_union,

    // Namespaces and aliases
    query_is_namespace,
    query_is_namespace_alias,
    query_is_type_alias,

    // Enums
    query_is_unscoped_enum,
    query_is_scoped_enum,

    // Enumerators
    query_is_enumerator,

    // Templates
    query_is_template,
    query_is_class_template,
    query_is_alias_template,
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
    query_is_template_parameter,
    query_is_type_template_parameter,
    query_is_nontype_template_parameter,
    query_is_template_template_parameter,
    query_has_default_argument,

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
    query_is_rvalue,
    query_is_value,

    // Traits
    query_get_decl_traits,
    query_get_linkage_traits,
    query_get_access_traits,
    query_get_type_traits,

    // Associated reflections
    query_get_entity,
    query_get_parent,
    query_get_type,
    query_get_return_type,
    query_get_this_ref_type,
    query_get_definition,

    // Traversal
    query_get_begin,
    query_get_next,

    // Type transformation
    query_remove_cv,
    query_remove_const,
    query_remove_volatile,
    query_add_cv,
    query_add_const,
    query_add_volatile,
    query_remove_reference,
    query_add_lvalue_reference,
    query_add_rvalue_reference,
    query_remove_pointer,
    query_add_pointer,
    query_remove_cvref,
    query_decay,
    query_make_signed,
    query_make_unsigned,

    // Name
    query_get_name,
    query_get_display_name,
};

#endif
