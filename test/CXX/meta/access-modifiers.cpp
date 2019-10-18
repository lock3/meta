// RUN: %clang_cc1 -freflection -std=c++2a %s

#include "reflection_query.h"
#include "reflection_mod.h"

constexpr meta::info front_member(meta::info reflection) {
  return __reflect(query_get_begin_member, reflection);
}

constexpr meta::info next_member(meta::info reflection) {
  return __reflect(query_get_next_member, reflection);
}

constexpr meta::info front_base(meta::info reflection) {
  return __reflect(query_get_begin_base_spec, reflection);
}

consteval bool is_public(meta::info base_or_mem) {
  return __reflect(query_is_public, base_or_mem);
}

consteval bool is_protected(meta::info base_or_mem) {
  return __reflect(query_is_protected, base_or_mem);
}

consteval bool is_private(meta::info base_or_mem) {
  return __reflect(query_is_private, base_or_mem);
}

consteval void make_default(meta::info& base_or_mem) {
  __reflect_mod(query_set_access, base_or_mem, AccessModifier::Default);
}

consteval void make_public(meta::info& base_or_mem) {
  __reflect_mod(query_set_access, base_or_mem, AccessModifier::Public);
}

consteval void make_protected(meta::info& base_or_mem) {
  __reflect_mod(query_set_access, base_or_mem, AccessModifier::Protected);
}

consteval void make_private(meta::info& base_or_mem) {
  __reflect_mod(query_set_access, base_or_mem, AccessModifier::Private);
}

consteval void clear_access_modifier(meta::info& base_or_mem) {
  __reflect_mod(query_set_access, base_or_mem, AccessModifier::NotModified);
}

namespace make_default_class_test {
  struct Base { };

  struct Existing : public Base {
    int field_1;

    int method_1() { return 0; }

    using type_alias_1 = int;
  };

  class New {
    consteval {
      meta::info refl = reflexpr(Existing);

      // Fields
      meta::info field_1 = front_member(refl);
      make_default(field_1);

      -> field_1;

      // Methods
      meta::info method_1 = next_member(field_1);
      make_default(method_1);

      -> method_1;

      // Type aliases
      meta::info type_alias_1 = next_member(method_1);
      make_default(type_alias_1);

      -> type_alias_1;

      // Bases
      meta::info base_1 = front_base(refl);
      make_default(base_1);

      -> base_1;
    }
  };

  constexpr meta::info refl = reflexpr(New);

  // Fields
  constexpr meta::info field_1 = front_member(refl);
  static_assert(is_private(field_1));

  // Methods
  constexpr meta::info method_1 = next_member(field_1);
  static_assert(is_private(method_1));

  // Type aliases
  constexpr meta::info type_alias_1 = next_member(method_1);
  static_assert(is_private(type_alias_1));

  // Bases
  constexpr meta::info base_1 = front_base(refl);
  static_assert(is_private(base_1));
}

namespace make_default_class_aborted_test {
  struct Base { };

  struct Existing : public Base {
    int field_1;

    int method_1() { return 0; }

    using type_alias_1 = int;
  };

  class New {
    consteval {
      meta::info refl = reflexpr(Existing);

      // Fields
      meta::info field_1 = front_member(refl);
      make_default(field_1);
      clear_access_modifier(field_1);

      -> field_1;

      // Methods
      meta::info method_1 = next_member(field_1);
      make_default(method_1);
      clear_access_modifier(method_1);

      -> method_1;

      // Type aliases
      meta::info type_alias_1 = next_member(method_1);
      make_default(type_alias_1);
      clear_access_modifier(type_alias_1);

      -> type_alias_1;

      // Bases
      meta::info base_1 = front_base(refl);
      make_default(base_1);
      clear_access_modifier(base_1);

      -> base_1;
    }
  };

  constexpr meta::info refl = reflexpr(New);

  // Fields
  constexpr meta::info field_1 = front_member(refl);
  static_assert(is_public(field_1));

  // Methods
  constexpr meta::info method_1 = next_member(field_1);
  static_assert(is_public(method_1));

  // Type aliases
  constexpr meta::info type_alias_1 = next_member(method_1);
  static_assert(is_public(type_alias_1));

  // Bases
  constexpr meta::info base_1 = front_base(refl);
  static_assert(is_public(base_1));
}

namespace make_default_struct_test {
  struct Base { };

  class Existing : private Base {
    int field_1;

    int method_1() { return 0; }

    using type_alias_1 = int;
  };

  struct New {
    consteval {
      meta::info refl = reflexpr(Existing);

      // Fields
      meta::info field_1 = front_member(refl);
      make_default(field_1);

      -> field_1;

      // Methods
      meta::info method_1 = next_member(field_1);
      make_default(method_1);

      -> method_1;

      // Type aliases
      meta::info type_alias_1 = next_member(method_1);
      make_default(type_alias_1);

      -> type_alias_1;

      // Bases
      meta::info base_1 = front_base(refl);
      make_default(base_1);

      -> base_1;
    }
  };

  constexpr meta::info refl = reflexpr(New);

  // Fields
  constexpr meta::info field_1 = front_member(refl);
  static_assert(is_public(field_1));

  // Methods
  constexpr meta::info method_1 = next_member(field_1);
  static_assert(is_public(method_1));

  // Type aliases
  constexpr meta::info type_alias_1 = next_member(method_1);
  static_assert(is_public(type_alias_1));

  // Bases
  constexpr meta::info base_1 = next_member(method_1);
  static_assert(is_public(base_1));
}

namespace make_default_struct_aborted_test {
  struct Base { };

  class Existing : private Base {
    int field_1;

    int method_1() { return 0; }

    using type_alias_1 = int;
  };

  struct New {
    consteval {
      meta::info refl = reflexpr(Existing);

      // Fields
      meta::info field_1 = front_member(refl);
      make_default(field_1);
      clear_access_modifier(field_1);

      -> field_1;

      // Methods
      meta::info method_1 = next_member(field_1);
      make_default(method_1);
      clear_access_modifier(method_1);

      -> method_1;

      // Type aliases
      meta::info type_alias_1 = next_member(method_1);
      make_default(type_alias_1);
      clear_access_modifier(type_alias_1);

      -> type_alias_1;

      // Bases
      meta::info base_1 = front_base(refl);
      make_default(base_1);
      clear_access_modifier(base_1);

      -> base_1;
    }
  };

  constexpr meta::info refl = reflexpr(New);

  // Fields
  constexpr meta::info field_1 = front_member(refl);
  static_assert(is_private(field_1));

  // Methods
  constexpr meta::info method_1 = next_member(field_1);
  static_assert(is_private(method_1));

  // Type aliases
  constexpr meta::info type_alias_1 = next_member(method_1);
  static_assert(is_private(type_alias_1));

  // Bases
  constexpr meta::info base_1 = front_base(refl);
  static_assert(is_private(base_1));
}

namespace make_public_test {
  struct Base { };

  class Existing : private Base {
    int field_1;

    int method_1() { return 0; }

    using type_alias_1 = int;
  };

  class New {
    consteval {
      meta::info refl = reflexpr(Existing);

      // Fields
      meta::info field_1 = front_member(refl);
      make_public(field_1);

      -> field_1;

      // Methods
      meta::info method_1 = next_member(field_1);
      make_public(method_1);

      -> method_1;

      // Type aliases
      meta::info type_alias_1 = next_member(method_1);
      make_public(type_alias_1);

      -> type_alias_1;

      // Bases
      meta::info base_1 = front_base(refl);
      make_public(base_1);

      -> base_1;
    }
  };

  constexpr meta::info refl = reflexpr(New);

  // Fields
  constexpr meta::info field_1 = front_member(refl);
  static_assert(is_public(field_1));

  // Methods
  constexpr meta::info method_1 = next_member(field_1);
  static_assert(is_public(method_1));

  // Type aliases
  constexpr meta::info type_alias_1 = next_member(method_1);
  static_assert(is_public(type_alias_1));

  // Bases
  constexpr meta::info base_1 = front_base(refl);
  static_assert(is_public(base_1));
}

namespace make_public_aborted_test {
  struct Base { };

  class Existing : private Base {
    int field_1;

    int method_1() { return 0; }

    using type_alias_1 = int;
  };

  class New {
    consteval {
      meta::info refl = reflexpr(Existing);

      // Fields
      meta::info field_1 = front_member(refl);
      make_public(field_1);
      clear_access_modifier(field_1);

      -> field_1;

      // Methods
      meta::info method_1 = next_member(field_1);
      make_public(method_1);
      clear_access_modifier(method_1);

      -> method_1;

      // Type aliases
      meta::info type_alias_1 = next_member(method_1);
      make_public(type_alias_1);
      clear_access_modifier(type_alias_1);

      -> type_alias_1;

      // Bases
      meta::info base_1 = front_base(refl);
      make_public(base_1);
      clear_access_modifier(base_1);

      -> base_1;
    }
  };

  constexpr meta::info refl = reflexpr(New);

  // Fields
  constexpr meta::info field_1 = front_member(refl);
  static_assert(is_private(field_1));

  // Methods
  constexpr meta::info method_1 = next_member(field_1);
  static_assert(is_private(method_1));

  // Type aliases
  constexpr meta::info type_alias_1 = next_member(method_1);
  static_assert(is_private(type_alias_1));

  // Bases
  constexpr meta::info base_1 = front_base(refl);
  static_assert(is_private(base_1));
}

namespace make_protected_test {
  struct Base { };

  class Existing : private Base {
    int field_1;

    int method_1() { return 0; }

    using type_alias_1 = int;
  };

  class New {
    consteval {
      meta::info refl = reflexpr(Existing);

      // Fields
      meta::info field_1 = front_member(refl);
      make_protected(field_1);

      -> field_1;

      // Methods
      meta::info method_1 = next_member(field_1);
      make_protected(method_1);

      -> method_1;

      // Type aliases
      meta::info type_alias_1 = next_member(method_1);
      make_protected(type_alias_1);

      -> type_alias_1;

      // Bases
      meta::info base_1 = front_base(refl);
      make_protected(base_1);

      -> base_1;
    }
  };

  constexpr meta::info refl = reflexpr(New);

  // Fields
  constexpr meta::info field_1 = front_member(reflexpr(New));
  static_assert(is_protected(field_1));

  // Methods
  constexpr meta::info method_1 = next_member(field_1);
  static_assert(is_protected(method_1));

  // Type aliases
  constexpr meta::info type_alias_1 = next_member(method_1);
  static_assert(is_protected(type_alias_1));

  // Bases
  constexpr meta::info base_1 = front_base(refl);
  static_assert(is_protected(base_1));
}

namespace make_protected_aborted_test {
  struct Base { };

  class Existing : private Base {
    int field_1;

    int method_1() { return 0; }

    using type_alias_1 = int;
  };

  class New {
    consteval {
      meta::info refl = reflexpr(Existing);

      // Fields
      meta::info field_1 = front_member(refl);
      make_protected(field_1);
      clear_access_modifier(field_1);

      -> field_1;

      // Methods
      meta::info method_1 = next_member(field_1);
      make_protected(method_1);
      clear_access_modifier(method_1);

      -> method_1;

      // Type aliases
      meta::info type_alias_1 = next_member(method_1);
      make_protected(type_alias_1);
      clear_access_modifier(type_alias_1);

      -> type_alias_1;

      // Bases
      meta::info base_1 = front_base(refl);
      make_protected(base_1);
      clear_access_modifier(base_1);

      -> base_1;
    }
  };

  constexpr meta::info refl = reflexpr(New);

  // Fields
  constexpr meta::info field_1 = front_member(reflexpr(New));
  static_assert(is_private(field_1));

  // Methods
  constexpr meta::info method_1 = next_member(field_1);
  static_assert(is_private(method_1));

  // Type aliases
  constexpr meta::info type_alias_1 = next_member(method_1);
  static_assert(is_private(type_alias_1));

  // Bases
  constexpr meta::info base_1 = front_base(refl);
  static_assert(is_private(base_1));
}

namespace make_private_test {
  struct Base { };

  struct Existing : public Base {
    int field_1;

    int method_1() { return 0; }

    using type_alias_1 = int;
  };

  struct New {
    consteval {
      meta::info refl = reflexpr(Existing);

      // Fields
      meta::info field_1 = front_member(refl);
      make_private(field_1);

      -> field_1;

      // Methods
      meta::info method_1 = next_member(field_1);
      make_private(method_1);

      -> method_1;

      // Type aliases
      meta::info type_alias_1 = next_member(method_1);
      make_private(type_alias_1);

      -> type_alias_1;

      // Bases
      meta::info base_1 = front_base(refl);
      make_private(base_1);

      -> base_1;
    }
  };

  constexpr meta::info refl = reflexpr(New);

  // Fields
  constexpr meta::info field_1 = front_member(reflexpr(New));
  static_assert(is_private(field_1));

  // Methods
  constexpr meta::info method_1 = next_member(field_1);
  static_assert(is_private(method_1));

  // Type aliases
  constexpr meta::info type_alias_1 = next_member(method_1);
  static_assert(is_private(type_alias_1));

  // Bases
  constexpr meta::info base_1 = front_base(refl);
  static_assert(is_private(base_1));
}

namespace make_private_aborted_test {
  struct Base { };

  struct Existing : public Base {
    int field_1;

    int method_1() { return 0; }

    using type_alias_1 = int;
  };

  struct New {
    consteval {
      meta::info refl = reflexpr(Existing);

      // Fields
      meta::info field_1 = front_member(refl);
      make_private(field_1);
      clear_access_modifier(field_1);

      -> field_1;

      // Methods
      meta::info method_1 = next_member(field_1);
      make_private(method_1);
      clear_access_modifier(method_1);

      -> method_1;

      // Type aliases
      meta::info type_alias_1 = next_member(method_1);
      make_private(type_alias_1);
      clear_access_modifier(type_alias_1);

      -> type_alias_1;

      // Bases
      meta::info base_1 = front_base(refl);
      make_default(base_1);
      clear_access_modifier(base_1);

      -> base_1;
    }
  };

  constexpr meta::info refl = reflexpr(New);

  // Fields
  constexpr meta::info field_1 = front_member(refl);
  static_assert(is_public(field_1));

  // Methods
  constexpr meta::info method_1 = next_member(field_1);
  static_assert(is_public(method_1));

  // Type aliases
  constexpr meta::info type_alias_1 = next_member(method_1);
  static_assert(is_public(type_alias_1));

  // Bases
  constexpr meta::info base_1 = front_base(refl);
  static_assert(is_public(base_1));

}

int main() {
  return 0;
}
