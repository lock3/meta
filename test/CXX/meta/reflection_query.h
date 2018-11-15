#ifndef REFLECTION_QUERY_H
#define REFLECTION_QUERY_H

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
  query_get_decl_traits,
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

#endif
