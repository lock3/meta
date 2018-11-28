#ifndef REFLECTION_TRAITS_H
#define REFLECTION_TRAITS_H

// -------------------------------------------------------------------------- //
// Specifiers and traits
//
// NOTE: These order and structure of the _info classes needs to be kept in
// sync with the compiler.

enum linkage_kind : unsigned {
  no_linkage,
  internal_linkage,
  external_linkage
};

// The linkage of a bitfield is always stored in the first 2 bits.
static constexpr linkage_kind get_linkage(unsigned n) {
  return linkage_kind(n & 0x03);
}

enum access_kind : unsigned {
  no_access,
  public_access,
  private_access,
  protected_access,
  default_access, // Not a real access specifier
};

// Access specifiers are always stored in bits 3 and 4.
static constexpr access_kind get_access(unsigned n) {
  return access_kind((n >> 2) & 0x03);
}

enum storage_kind : unsigned {
  automatic_storage,
  static_storage,
  thread_storage,
  allocated_storage
};

// When present, storage specifiers are stored in bits 5 and 6.
//
// FIXME: This isn't accurate.
static constexpr storage_kind get_storage(unsigned n) {
  return storage_kind((n >> 4) & 0x03);
}

// All named declarations have linkage and access.
struct decl_traits {
  constexpr explicit decl_traits(unsigned n)
    : linkage      (get_linkage(n)), // 0x01 | 0x02
      access       (get_access(n))   // 0x04 | 0x08
  { }

  linkage_kind linkage : 2;
  access_kind access : 2;
};

struct variable_traits {
  constexpr explicit variable_traits(unsigned n)
    : linkage      (get_linkage(n)), // 0x01 | 0x02
      access       (get_access(n)),  // 0x04 | 0x08
      storage      (get_storage(n)), // 0x10 | 0x20
      is_constexpr (n & 0x40),
      is_defined   (n & 0x80),
      is_inline    (n & 0x0100)
  { }

  linkage_kind linkage : 2;
  access_kind access : 2;
  storage_kind storage : 2;
  bool is_constexpr : 1;
  bool is_defined : 1;
  bool is_inline : 1;
};

struct function_traits {
  constexpr explicit function_traits(unsigned n)
    : linkage      (get_linkage(n)), // 0x01 | 0x02
      access       (get_access(n)),  // 0x04 | 0x08
      is_constexpr (n & 0x10),
      is_noexcept  (n & 0x20),
      is_defined   (n & 0x40),
      is_inline    (n & 0x80),
      is_deleted   (n & 0x0100)
  { }

  linkage_kind linkage : 2;
  access_kind access : 2;
  bool is_constexpr : 1;
  bool is_noexcept : 1;
  bool is_defined : 1;
  bool is_inline : 1;
  bool is_deleted : 1;
};

struct value_traits {
  constexpr explicit value_traits(unsigned n)
    : linkage      (get_linkage(n)), // 0x01 | 0x02
      access       (get_access(n))   // 0x04 | 0x08
  { }

  linkage_kind linkage : 2;
  access_kind access : 2;
};

struct namespace_traits {
  constexpr explicit namespace_traits(unsigned n)
    : linkage      (get_linkage(n)), // 0x01 | 0x02
      access       (get_access(n)),  // 0x04 | 0x08
      is_inline    (n & 0x10)
  { }

  linkage_kind linkage : 2;
  access_kind access : 2;
  bool is_inline : 1;
};

struct field_traits {
  constexpr explicit field_traits(unsigned n)
    : linkage      (get_linkage(n)), // 0x01 | 0x02
      access       (get_access(n)),  // 0x04 | 0x08
      is_mutable   (n & 0x10)
  { }

  linkage_kind linkage : 2;
  access_kind access : 2;
  bool is_mutable : 1;
};


// Methods

enum method_kind : unsigned {
  method_normal,
  method_ctor,
  method_dtor,
  method_conv
};

// For methods, the kind is stored in bits 5 and 6.
static constexpr method_kind get_method(unsigned n) {
  return method_kind((n >> 4) & 0x03);
}

struct method_traits {
  constexpr explicit method_traits(unsigned n)
    : linkage        (get_linkage(n)), // 0x01 | 0x02
      access         (get_access(n)),  // 0x04 | 0x08
      kind           (get_method(n)),  // 0x10 | 0x20
      is_constexpr   (n & 0x40),
      is_explicit    (n & 0x80),
      is_virtual     (n & 0x100),
      is_pure        (n & 0x200),
      is_final       (n & 0x400),
      is_override    (n & 0x800),
      is_noexcept    (n & 0x1000),
      is_defined     (n & 0x2000),
      is_inline      (n & 0x4000),
      is_deleted     (n & 0x8000),
      is_defaulted   (n & 0x10000),
      is_trivial     (n & 0x20000),
      is_default_ctor(n & 0x40000),
      is_copy_ctor   (n & 0x80000),
      is_move_ctor   (n & 0x100000),
      is_copy_assign (n & 0x200000),
      is_move_assign (n & 0x400000)
  { }

  linkage_kind linkage : 2;
  access_kind access : 2;
  method_kind kind : 2;
  bool is_constexpr : 1;
  bool is_explicit : 1;
  bool is_virtual : 1;
  bool is_pure : 1;
  bool is_final : 1;
  bool is_override : 1;
  bool is_noexcept : 1;
  bool is_defined : 1;
  bool is_inline : 1;
  bool is_deleted : 1;
  bool is_defaulted : 1;
  bool is_trivial : 1;
  bool is_default_ctor : 1;
  bool is_copy_ctor : 1;
  bool is_move_ctor : 1;
  bool is_copy_assign : 1;
  bool is_move_assign : 1;
};

// Basic Traits
struct linkage_traits {
  constexpr explicit linkage_traits(unsigned n)
    : kind(get_linkage(n)) { }

  linkage_kind kind : 2;
};

struct access_traits {
  constexpr explicit access_traits(unsigned n)
    : kind(get_access(n)) { }

  access_kind kind : 2;
};

// Classes

// Kinds of class types.
enum class_kind {
  struct_class_kind,
  class_class_kind,
  union_class_kind
};

// For classes, the kind is stored in bits 5 and 6.
static constexpr class_kind get_class_kind(unsigned n) {
  return class_kind((n >> 4) & 0x03);
}

// TODO: Accumulate all known type traits for classes.
struct class_traits {
  constexpr explicit class_traits(unsigned n)
    : linkage       (get_linkage(n)),    // 0x01 | 0x02
      access        (get_access(n)),     // 0x04 | 0x08
      kind          (get_class_kind(n)), // 0x10 | 0x20
      is_complete   (n & 0x40),
      is_polymorphic(n & 0x80),
      is_abstract   (n & 0x100),
      is_final      (n & 0x200),
      is_empty      (n & 0x400)
  { }

  linkage_kind linkage : 2;
  access_kind access : 2;
  class_kind kind : 2;
  bool is_complete : 1;
  bool is_polymorphic : 1;
  bool is_abstract : 1;
  bool is_final : 1;
  bool is_empty : 1;
};

struct enum_traits {
  constexpr explicit enum_traits(unsigned n)
    : linkage    (get_linkage(n)), // 0x01 | 0x02
      access     (get_access(n)),  // 0x04 | 0x08
      is_scoped  (n & 0x10),
      is_complete(n & 0x20)
  { }

  linkage_kind linkage : 2;
  access_kind access : 2;
  bool is_scoped : 1;
  bool is_complete : 1;
};

#endif
