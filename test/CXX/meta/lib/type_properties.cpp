// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

class forward_declared_class;

constexpr meta::info forward_class_refl = reflexpr(forward_declared_class);
static_assert(__reflect(query_is_incomplete_type, forward_class_refl));
static_assert(!__reflect(query_is_const_type, forward_class_refl));
static_assert(!__reflect(query_is_volatile_type, forward_class_refl));
static_assert(!__reflect(query_is_trivial_type, forward_class_refl));
static_assert(!__reflect(query_is_trivially_copyable_type, forward_class_refl));
static_assert(!__reflect(query_is_standard_layout_type, forward_class_refl));
static_assert(!__reflect(query_is_pod_type, forward_class_refl));
static_assert(!__reflect(query_is_literal_type, forward_class_refl));
static_assert(!__reflect(query_is_empty_type, forward_class_refl));
static_assert(!__reflect(query_is_polymorphic_type, forward_class_refl));
static_assert(!__reflect(query_is_abstract_type, forward_class_refl));
static_assert(!__reflect(query_is_final_type, forward_class_refl));
static_assert(!__reflect(query_is_aggregate_type, forward_class_refl));
static_assert(!__reflect(query_is_signed_type, forward_class_refl));
static_assert(!__reflect(query_is_unsigned_type, forward_class_refl));
static_assert(!__reflect(query_has_unique_object_representations_type, forward_class_refl));

const int w = 0;

constexpr meta::info const_var_refl = reflexpr(w);
static_assert(!__reflect(query_is_incomplete_type, const_var_refl));
static_assert(!__reflect(query_is_const_type, const_var_refl));
static_assert(!__reflect(query_is_volatile_type, const_var_refl));
static_assert(!__reflect(query_is_trivial_type, const_var_refl));
static_assert(!__reflect(query_is_trivially_copyable_type, const_var_refl));
static_assert(!__reflect(query_is_standard_layout_type, const_var_refl));
static_assert(!__reflect(query_is_pod_type, const_var_refl));
static_assert(!__reflect(query_is_literal_type, const_var_refl));
static_assert(!__reflect(query_is_empty_type, const_var_refl));
static_assert(!__reflect(query_is_polymorphic_type, const_var_refl));
static_assert(!__reflect(query_is_abstract_type, const_var_refl));
static_assert(!__reflect(query_is_final_type, const_var_refl));
static_assert(!__reflect(query_is_aggregate_type, const_var_refl));
static_assert(!__reflect(query_is_signed_type, const_var_refl));
static_assert(!__reflect(query_is_unsigned_type, const_var_refl));
static_assert(!__reflect(query_has_unique_object_representations_type, const_var_refl));

constexpr meta::info const_var_type_refl = __reflect(query_get_type, const_var_refl);
static_assert(!__reflect(query_is_incomplete_type, const_var_type_refl));
static_assert(__reflect(query_is_const_type, const_var_type_refl));
static_assert(!__reflect(query_is_volatile_type, const_var_type_refl));
static_assert(__reflect(query_is_trivial_type, const_var_type_refl));
static_assert(__reflect(query_is_trivially_copyable_type, const_var_type_refl));
static_assert(__reflect(query_is_standard_layout_type, const_var_type_refl));
static_assert(__reflect(query_is_pod_type, const_var_type_refl));
static_assert(__reflect(query_is_literal_type, const_var_type_refl));
static_assert(!__reflect(query_is_empty_type, const_var_type_refl));
static_assert(!__reflect(query_is_polymorphic_type, const_var_type_refl));
static_assert(!__reflect(query_is_abstract_type, const_var_type_refl));
static_assert(!__reflect(query_is_final_type, const_var_type_refl));
static_assert(!__reflect(query_is_aggregate_type, const_var_type_refl));
static_assert(__reflect(query_is_signed_type, const_var_type_refl));
static_assert(!__reflect(query_is_unsigned_type, const_var_type_refl));
static_assert(__reflect(query_has_unique_object_representations_type, const_var_type_refl));

volatile int x = 0;

constexpr meta::info volatile_var_refl = reflexpr(x);
static_assert(!__reflect(query_is_incomplete_type, volatile_var_refl));
static_assert(!__reflect(query_is_const_type, volatile_var_refl));
static_assert(!__reflect(query_is_volatile_type, volatile_var_refl));
static_assert(!__reflect(query_is_trivial_type, volatile_var_refl));
static_assert(!__reflect(query_is_trivially_copyable_type, volatile_var_refl));
static_assert(!__reflect(query_is_standard_layout_type, volatile_var_refl));
static_assert(!__reflect(query_is_pod_type, volatile_var_refl));
static_assert(!__reflect(query_is_literal_type, volatile_var_refl));
static_assert(!__reflect(query_is_empty_type, volatile_var_refl));
static_assert(!__reflect(query_is_polymorphic_type, volatile_var_refl));
static_assert(!__reflect(query_is_abstract_type, volatile_var_refl));
static_assert(!__reflect(query_is_final_type, volatile_var_refl));
static_assert(!__reflect(query_is_aggregate_type, volatile_var_refl));
static_assert(!__reflect(query_is_signed_type, volatile_var_refl));
static_assert(!__reflect(query_is_unsigned_type, volatile_var_refl));
static_assert(!__reflect(query_has_unique_object_representations_type, volatile_var_refl));

constexpr meta::info volatile_var_type_refl =  __reflect(query_get_type, volatile_var_refl);
static_assert(!__reflect(query_is_incomplete_type, volatile_var_type_refl));
static_assert(!__reflect(query_is_const_type, volatile_var_type_refl));
static_assert(__reflect(query_is_volatile_type, volatile_var_type_refl));
static_assert(__reflect(query_is_trivial_type, volatile_var_type_refl));
static_assert(__reflect(query_is_trivially_copyable_type, volatile_var_type_refl));
static_assert(__reflect(query_is_standard_layout_type, volatile_var_type_refl));
static_assert(__reflect(query_is_pod_type, volatile_var_type_refl));
static_assert(__reflect(query_is_literal_type, volatile_var_type_refl));
static_assert(!__reflect(query_is_empty_type, volatile_var_type_refl));
static_assert(!__reflect(query_is_polymorphic_type, volatile_var_type_refl));
static_assert(!__reflect(query_is_abstract_type, volatile_var_type_refl));
static_assert(!__reflect(query_is_final_type, volatile_var_type_refl));
static_assert(!__reflect(query_is_aggregate_type, volatile_var_type_refl));
static_assert(__reflect(query_is_signed_type, volatile_var_type_refl));
static_assert(!__reflect(query_is_unsigned_type, volatile_var_type_refl));
static_assert(__reflect(query_has_unique_object_representations_type, volatile_var_type_refl));

unsigned y = 0;

constexpr meta::info unsigned_var_refl = reflexpr(y);
static_assert(!__reflect(query_is_incomplete_type, unsigned_var_refl));
static_assert(!__reflect(query_is_const_type, unsigned_var_refl));
static_assert(!__reflect(query_is_volatile_type, unsigned_var_refl));
static_assert(!__reflect(query_is_trivial_type, unsigned_var_refl));
static_assert(!__reflect(query_is_trivially_copyable_type, unsigned_var_refl));
static_assert(!__reflect(query_is_standard_layout_type, unsigned_var_refl));
static_assert(!__reflect(query_is_pod_type, unsigned_var_refl));
static_assert(!__reflect(query_is_literal_type, unsigned_var_refl));
static_assert(!__reflect(query_is_empty_type, unsigned_var_refl));
static_assert(!__reflect(query_is_polymorphic_type, unsigned_var_refl));
static_assert(!__reflect(query_is_abstract_type, unsigned_var_refl));
static_assert(!__reflect(query_is_final_type, unsigned_var_refl));
static_assert(!__reflect(query_is_aggregate_type, unsigned_var_refl));
static_assert(!__reflect(query_is_signed_type, unsigned_var_refl));
static_assert(!__reflect(query_is_unsigned_type, unsigned_var_refl));
static_assert(!__reflect(query_has_unique_object_representations_type, unsigned_var_refl));

constexpr meta::info unsigned_var_type_refl = __reflect(query_get_type, unsigned_var_refl);
static_assert(!__reflect(query_is_incomplete_type, unsigned_var_type_refl));
static_assert(!__reflect(query_is_const_type, unsigned_var_type_refl));
static_assert(!__reflect(query_is_volatile_type, unsigned_var_type_refl));
static_assert(__reflect(query_is_trivial_type, unsigned_var_type_refl));
static_assert(__reflect(query_is_trivially_copyable_type, unsigned_var_type_refl));
static_assert(__reflect(query_is_standard_layout_type, unsigned_var_type_refl));
static_assert(__reflect(query_is_pod_type, unsigned_var_type_refl));
static_assert(__reflect(query_is_literal_type, unsigned_var_type_refl));
static_assert(!__reflect(query_is_empty_type, unsigned_var_type_refl));
static_assert(!__reflect(query_is_polymorphic_type, unsigned_var_type_refl));
static_assert(!__reflect(query_is_abstract_type, unsigned_var_type_refl));
static_assert(!__reflect(query_is_final_type, unsigned_var_type_refl));
static_assert(!__reflect(query_is_aggregate_type, unsigned_var_type_refl));
static_assert(!__reflect(query_is_signed_type, unsigned_var_type_refl));
static_assert(__reflect(query_is_unsigned_type, unsigned_var_type_refl));
static_assert(__reflect(query_has_unique_object_representations_type, unsigned_var_type_refl));

float z = 0;

constexpr meta::info float_var_refl = reflexpr(z);
static_assert(!__reflect(query_is_incomplete_type, float_var_refl));
static_assert(!__reflect(query_is_const_type, float_var_refl));
static_assert(!__reflect(query_is_volatile_type, float_var_refl));
static_assert(!__reflect(query_is_trivial_type, float_var_refl));
static_assert(!__reflect(query_is_trivially_copyable_type, float_var_refl));
static_assert(!__reflect(query_is_standard_layout_type, float_var_refl));
static_assert(!__reflect(query_is_pod_type, float_var_refl));
static_assert(!__reflect(query_is_literal_type, float_var_refl));
static_assert(!__reflect(query_is_empty_type, float_var_refl));
static_assert(!__reflect(query_is_polymorphic_type, float_var_refl));
static_assert(!__reflect(query_is_abstract_type, float_var_refl));
static_assert(!__reflect(query_is_final_type, float_var_refl));
static_assert(!__reflect(query_is_aggregate_type, float_var_refl));
static_assert(!__reflect(query_is_signed_type, float_var_refl));
static_assert(!__reflect(query_is_unsigned_type, float_var_refl));
static_assert(!__reflect(query_has_unique_object_representations_type, float_var_refl));

constexpr meta::info float_var_type_refl = __reflect(query_get_type, float_var_refl);
static_assert(!__reflect(query_is_incomplete_type, float_var_type_refl));
static_assert(!__reflect(query_is_const_type, float_var_type_refl));
static_assert(!__reflect(query_is_volatile_type, float_var_type_refl));
static_assert(__reflect(query_is_trivial_type, float_var_type_refl));
static_assert(__reflect(query_is_trivially_copyable_type, float_var_type_refl));
static_assert(__reflect(query_is_standard_layout_type, float_var_type_refl));
static_assert(__reflect(query_is_pod_type, float_var_type_refl));
static_assert(__reflect(query_is_literal_type, float_var_type_refl));
static_assert(!__reflect(query_is_empty_type, float_var_type_refl));
static_assert(!__reflect(query_is_polymorphic_type, float_var_type_refl));
static_assert(!__reflect(query_is_abstract_type, float_var_type_refl));
static_assert(!__reflect(query_is_final_type, float_var_type_refl));
static_assert(!__reflect(query_is_aggregate_type, float_var_type_refl));
static_assert(__reflect(query_is_signed_type, float_var_type_refl));
static_assert(!__reflect(query_is_unsigned_type, float_var_type_refl));
static_assert(!__reflect(query_has_unique_object_representations_type, float_var_type_refl));

class empty_class {
};

constexpr meta::info empty_class_refl = reflexpr(empty_class);
static_assert(!__reflect(query_is_incomplete_type, empty_class_refl));
static_assert(!__reflect(query_is_const_type, empty_class_refl));
static_assert(!__reflect(query_is_volatile_type, empty_class_refl));
static_assert(__reflect(query_is_trivial_type, empty_class_refl));
static_assert(__reflect(query_is_trivially_copyable_type, empty_class_refl));
static_assert(__reflect(query_is_standard_layout_type, empty_class_refl));
static_assert(__reflect(query_is_pod_type, empty_class_refl));
static_assert(__reflect(query_is_literal_type, empty_class_refl));
static_assert(__reflect(query_is_empty_type, empty_class_refl));
static_assert(!__reflect(query_is_polymorphic_type, empty_class_refl));
static_assert(!__reflect(query_is_abstract_type, empty_class_refl));
static_assert(!__reflect(query_is_final_type, empty_class_refl));
static_assert(__reflect(query_is_aggregate_type, empty_class_refl));
static_assert(!__reflect(query_is_signed_type, empty_class_refl));
static_assert(!__reflect(query_is_unsigned_type, empty_class_refl));
static_assert(!__reflect(query_has_unique_object_representations_type, empty_class_refl));

class abstract_class {
public:
  virtual ~abstract_class() { };

  virtual void do_thing() = 0;
};

constexpr meta::info abstract_class_refl = reflexpr(abstract_class);
static_assert(!__reflect(query_is_incomplete_type, abstract_class_refl));
static_assert(!__reflect(query_is_const_type, abstract_class_refl));
static_assert(!__reflect(query_is_volatile_type, abstract_class_refl));
static_assert(!__reflect(query_is_trivial_type, abstract_class_refl));
static_assert(!__reflect(query_is_trivially_copyable_type, abstract_class_refl));
static_assert(!__reflect(query_is_standard_layout_type, abstract_class_refl));
static_assert(!__reflect(query_is_pod_type, abstract_class_refl));
static_assert(!__reflect(query_is_literal_type, abstract_class_refl));
static_assert(!__reflect(query_is_empty_type, abstract_class_refl));
static_assert(__reflect(query_is_polymorphic_type, abstract_class_refl));
static_assert(__reflect(query_is_abstract_type, abstract_class_refl));
static_assert(!__reflect(query_is_final_type, abstract_class_refl));
static_assert(!__reflect(query_is_aggregate_type, abstract_class_refl));
static_assert(!__reflect(query_is_signed_type, abstract_class_refl));
static_assert(!__reflect(query_is_unsigned_type, abstract_class_refl));
static_assert(!__reflect(query_has_unique_object_representations_type, abstract_class_refl));

class child_class : abstract_class {
};

constexpr meta::info child_class_refl = reflexpr(child_class);
static_assert(!__reflect(query_is_incomplete_type, child_class_refl));
static_assert(!__reflect(query_is_const_type, child_class_refl));
static_assert(!__reflect(query_is_volatile_type, child_class_refl));
static_assert(!__reflect(query_is_trivial_type, child_class_refl));
static_assert(!__reflect(query_is_trivially_copyable_type, child_class_refl));
static_assert(!__reflect(query_is_standard_layout_type, child_class_refl));
static_assert(!__reflect(query_is_pod_type, child_class_refl));
static_assert(!__reflect(query_is_literal_type, child_class_refl));
static_assert(!__reflect(query_is_empty_type, child_class_refl));
static_assert(__reflect(query_is_polymorphic_type, child_class_refl));
static_assert(__reflect(query_is_abstract_type, child_class_refl));
static_assert(!__reflect(query_is_final_type, child_class_refl));
static_assert(!__reflect(query_is_aggregate_type, child_class_refl));
static_assert(!__reflect(query_is_signed_type, child_class_refl));
static_assert(!__reflect(query_is_unsigned_type, child_class_refl));
static_assert(!__reflect(query_has_unique_object_representations_type, child_class_refl));

class polymorphic_class {
  virtual void do_thing() { }
};

constexpr meta::info polymorphic_class_refl = reflexpr(polymorphic_class);
static_assert(!__reflect(query_is_incomplete_type, polymorphic_class_refl));
static_assert(!__reflect(query_is_const_type, polymorphic_class_refl));
static_assert(!__reflect(query_is_volatile_type, polymorphic_class_refl));
static_assert(!__reflect(query_is_trivial_type, polymorphic_class_refl));
static_assert(!__reflect(query_is_trivially_copyable_type, polymorphic_class_refl));
static_assert(!__reflect(query_is_standard_layout_type, polymorphic_class_refl));
static_assert(!__reflect(query_is_pod_type, polymorphic_class_refl));
static_assert(__reflect(query_is_literal_type, polymorphic_class_refl));
static_assert(!__reflect(query_is_empty_type, polymorphic_class_refl));
static_assert(__reflect(query_is_polymorphic_type, polymorphic_class_refl));
static_assert(!__reflect(query_is_abstract_type, polymorphic_class_refl));
static_assert(!__reflect(query_is_final_type, polymorphic_class_refl));
static_assert(!__reflect(query_is_aggregate_type, polymorphic_class_refl));
static_assert(!__reflect(query_is_signed_type, polymorphic_class_refl));
static_assert(!__reflect(query_is_unsigned_type, polymorphic_class_refl));
static_assert(!__reflect(query_has_unique_object_representations_type, polymorphic_class_refl));

class final_child_class final : polymorphic_class {
};

constexpr meta::info final_child_class_refl = reflexpr(final_child_class);
static_assert(!__reflect(query_is_incomplete_type, final_child_class_refl));
static_assert(!__reflect(query_is_const_type, final_child_class_refl));
static_assert(!__reflect(query_is_volatile_type, final_child_class_refl));
static_assert(!__reflect(query_is_trivial_type, final_child_class_refl));
static_assert(!__reflect(query_is_trivially_copyable_type, final_child_class_refl));
static_assert(!__reflect(query_is_standard_layout_type, final_child_class_refl));
static_assert(!__reflect(query_is_pod_type, final_child_class_refl));
static_assert(__reflect(query_is_literal_type, final_child_class_refl));
static_assert(!__reflect(query_is_empty_type, final_child_class_refl));
static_assert(__reflect(query_is_polymorphic_type, final_child_class_refl));
static_assert(!__reflect(query_is_abstract_type, final_child_class_refl));
static_assert(__reflect(query_is_final_type, final_child_class_refl));
static_assert(!__reflect(query_is_aggregate_type, final_child_class_refl));
static_assert(!__reflect(query_is_signed_type, final_child_class_refl));
static_assert(!__reflect(query_is_unsigned_type, final_child_class_refl));
static_assert(!__reflect(query_has_unique_object_representations_type, final_child_class_refl));

class non_aggregate_type {
public:
  non_aggregate_type() { }
};

constexpr meta::info non_aggregate_type_refl = reflexpr(non_aggregate_type);
static_assert(!__reflect(query_is_incomplete_type, non_aggregate_type_refl));
static_assert(!__reflect(query_is_const_type, non_aggregate_type_refl));
static_assert(!__reflect(query_is_volatile_type, non_aggregate_type_refl));
static_assert(!__reflect(query_is_trivial_type, non_aggregate_type_refl));
static_assert(__reflect(query_is_trivially_copyable_type, non_aggregate_type_refl));
static_assert(__reflect(query_is_standard_layout_type, non_aggregate_type_refl));
static_assert(!__reflect(query_is_pod_type, non_aggregate_type_refl));
static_assert(!__reflect(query_is_literal_type, non_aggregate_type_refl));
static_assert(__reflect(query_is_empty_type, non_aggregate_type_refl));
static_assert(!__reflect(query_is_polymorphic_type, non_aggregate_type_refl));
static_assert(!__reflect(query_is_abstract_type, non_aggregate_type_refl));
static_assert(!__reflect(query_is_final_type, non_aggregate_type_refl));
static_assert(!__reflect(query_is_aggregate_type, non_aggregate_type_refl));
static_assert(!__reflect(query_is_signed_type, non_aggregate_type_refl));
static_assert(!__reflect(query_is_unsigned_type, non_aggregate_type_refl));
static_assert(!__reflect(query_has_unique_object_representations_type, non_aggregate_type_refl));


class non_aggregate_copyable_type {
public:
  non_aggregate_copyable_type() = default;
  non_aggregate_copyable_type(const non_aggregate_copyable_type& ty) { }
};

constexpr meta::info non_aggregate_copyable_type_refl = reflexpr(non_aggregate_copyable_type);
static_assert(!__reflect(query_is_incomplete_type, non_aggregate_copyable_type_refl));
static_assert(!__reflect(query_is_const_type, non_aggregate_copyable_type_refl));
static_assert(!__reflect(query_is_volatile_type, non_aggregate_copyable_type_refl));
static_assert(!__reflect(query_is_trivial_type, non_aggregate_copyable_type_refl));
static_assert(!__reflect(query_is_trivially_copyable_type, non_aggregate_copyable_type_refl));
static_assert(__reflect(query_is_standard_layout_type, non_aggregate_copyable_type_refl));
static_assert(!__reflect(query_is_pod_type, non_aggregate_copyable_type_refl));
static_assert(__reflect(query_is_literal_type, non_aggregate_copyable_type_refl));
static_assert(__reflect(query_is_empty_type, non_aggregate_copyable_type_refl));
static_assert(!__reflect(query_is_polymorphic_type, non_aggregate_copyable_type_refl));
static_assert(!__reflect(query_is_abstract_type, non_aggregate_copyable_type_refl));
static_assert(!__reflect(query_is_final_type, non_aggregate_copyable_type_refl));
static_assert(!__reflect(query_is_aggregate_type, non_aggregate_copyable_type_refl));
static_assert(!__reflect(query_is_signed_type, non_aggregate_copyable_type_refl));
static_assert(!__reflect(query_is_unsigned_type, non_aggregate_copyable_type_refl));
static_assert(!__reflect(query_has_unique_object_representations_type, non_aggregate_copyable_type_refl));

class class_with_static_members {
  static int member;
};

constexpr meta::info class_with_static_members_type_refl = reflexpr(class_with_static_members);
static_assert(!__reflect(query_is_incomplete_type, class_with_static_members_type_refl));
static_assert(!__reflect(query_is_const_type, class_with_static_members_type_refl));
static_assert(!__reflect(query_is_volatile_type, class_with_static_members_type_refl));
static_assert(__reflect(query_is_trivial_type, class_with_static_members_type_refl));
static_assert(__reflect(query_is_trivially_copyable_type, class_with_static_members_type_refl));
static_assert(__reflect(query_is_standard_layout_type, class_with_static_members_type_refl));
static_assert(__reflect(query_is_pod_type, class_with_static_members_type_refl));
static_assert(__reflect(query_is_literal_type, class_with_static_members_type_refl));
static_assert(__reflect(query_is_empty_type, class_with_static_members_type_refl));
static_assert(!__reflect(query_is_polymorphic_type, class_with_static_members_type_refl));
static_assert(!__reflect(query_is_abstract_type, class_with_static_members_type_refl));
static_assert(!__reflect(query_is_final_type, class_with_static_members_type_refl));
static_assert(__reflect(query_is_aggregate_type, class_with_static_members_type_refl));
static_assert(!__reflect(query_is_signed_type, class_with_static_members_type_refl));
static_assert(!__reflect(query_is_unsigned_type, class_with_static_members_type_refl));
static_assert(!__reflect(query_has_unique_object_representations_type, class_with_static_members_type_refl));

class trivially_copyable_constexpr_class {
  int x;

public:
  constexpr trivially_copyable_constexpr_class() : x(1) { }
};

constexpr meta::info trivially_copyable_constexpr_class_type_refl = reflexpr(trivially_copyable_constexpr_class);
static_assert(!__reflect(query_is_incomplete_type, trivially_copyable_constexpr_class_type_refl));
static_assert(!__reflect(query_is_const_type, trivially_copyable_constexpr_class_type_refl));
static_assert(!__reflect(query_is_volatile_type, trivially_copyable_constexpr_class_type_refl));
static_assert(!__reflect(query_is_trivial_type, trivially_copyable_constexpr_class_type_refl));
static_assert(__reflect(query_is_trivially_copyable_type, trivially_copyable_constexpr_class_type_refl));
static_assert(__reflect(query_is_standard_layout_type, trivially_copyable_constexpr_class_type_refl));
static_assert(!__reflect(query_is_pod_type, trivially_copyable_constexpr_class_type_refl));
static_assert(__reflect(query_is_literal_type, trivially_copyable_constexpr_class_type_refl));
static_assert(!__reflect(query_is_empty_type, trivially_copyable_constexpr_class_type_refl));
static_assert(!__reflect(query_is_polymorphic_type, trivially_copyable_constexpr_class_type_refl));
static_assert(!__reflect(query_is_abstract_type, trivially_copyable_constexpr_class_type_refl));
static_assert(!__reflect(query_is_final_type, trivially_copyable_constexpr_class_type_refl));
static_assert(!__reflect(query_is_aggregate_type, trivially_copyable_constexpr_class_type_refl));
static_assert(!__reflect(query_is_signed_type, trivially_copyable_constexpr_class_type_refl));
static_assert(!__reflect(query_is_unsigned_type, trivially_copyable_constexpr_class_type_refl));
static_assert(__reflect(query_has_unique_object_representations_type, trivially_copyable_constexpr_class_type_refl));

class constexpr_class {
  int x;

public:
  constexpr constexpr_class() : x(1) { }
  constexpr constexpr_class(const constexpr_class &c) : x(c.x) { }
};

constexpr meta::info constexpr_class_type_refl = reflexpr(constexpr_class);
static_assert(!__reflect(query_is_incomplete_type, constexpr_class_type_refl));
static_assert(!__reflect(query_is_const_type, constexpr_class_type_refl));
static_assert(!__reflect(query_is_volatile_type, constexpr_class_type_refl));
static_assert(!__reflect(query_is_trivial_type, constexpr_class_type_refl));
static_assert(!__reflect(query_is_trivially_copyable_type, constexpr_class_type_refl));
static_assert(__reflect(query_is_standard_layout_type, constexpr_class_type_refl));
static_assert(!__reflect(query_is_pod_type, constexpr_class_type_refl));
static_assert(__reflect(query_is_literal_type, constexpr_class_type_refl));
static_assert(!__reflect(query_is_empty_type, constexpr_class_type_refl));
static_assert(!__reflect(query_is_polymorphic_type, constexpr_class_type_refl));
static_assert(!__reflect(query_is_abstract_type, constexpr_class_type_refl));
static_assert(!__reflect(query_is_final_type, constexpr_class_type_refl));
static_assert(!__reflect(query_is_aggregate_type, constexpr_class_type_refl));
static_assert(!__reflect(query_is_signed_type, constexpr_class_type_refl));
static_assert(!__reflect(query_is_unsigned_type, constexpr_class_type_refl));
static_assert(!__reflect(query_has_unique_object_representations_type, constexpr_class_type_refl));
