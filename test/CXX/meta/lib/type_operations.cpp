// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

constexpr meta::info void_refl = reflexpr(void);
static_assert(!__reflect(query_is_constructible, void_refl));
static_assert(!__reflect(query_is_trivially_constructible, void_refl));
static_assert(!__reflect(query_is_nothrow_constructible, void_refl));
static_assert(!__reflect(query_is_assignable, void_refl, void_refl));
static_assert(!__reflect(query_is_trivially_assignable, void_refl, void_refl));
static_assert(!__reflect(query_is_nothrow_assignable, void_refl, void_refl));
static_assert(!__reflect(query_is_destructible, void_refl));
static_assert(!__reflect(query_is_trivially_destructible, void_refl));
static_assert(!__reflect(query_is_nothrow_destructible, void_refl));

struct constructible {
  constructible() { }
  ~constructible() { }
  void operator=(const constructible &c) { }
};

constexpr meta::info constructible_class_refl = reflexpr(constructible);
static_assert(__reflect(query_is_constructible, constructible_class_refl));
static_assert(!__reflect(query_is_constructible, constructible_class_refl, reflexpr(int)));
static_assert(!__reflect(query_is_trivially_constructible, constructible_class_refl));
static_assert(!__reflect(query_is_trivially_constructible, constructible_class_refl, reflexpr(int)));
static_assert(!__reflect(query_is_nothrow_constructible, constructible_class_refl));
static_assert(!__reflect(query_is_nothrow_constructible, constructible_class_refl, reflexpr(int)));
static_assert(__reflect(query_is_assignable, constructible_class_refl, constructible_class_refl));
static_assert(!__reflect(query_is_assignable, constructible_class_refl, reflexpr(int)));
static_assert(!__reflect(query_is_trivially_assignable, constructible_class_refl, constructible_class_refl));
static_assert(!__reflect(query_is_trivially_assignable, constructible_class_refl, reflexpr(int)));
static_assert(!__reflect(query_is_nothrow_assignable, constructible_class_refl, constructible_class_refl));
static_assert(!__reflect(query_is_nothrow_assignable, constructible_class_refl, reflexpr(int)));
static_assert(__reflect(query_is_destructible, constructible_class_refl));
static_assert(!__reflect(query_is_trivially_destructible, constructible_class_refl));
static_assert(__reflect(query_is_nothrow_destructible, constructible_class_refl));

struct trivially_constructible {
};

constexpr meta::info trivially_constructible_class_refl = reflexpr(trivially_constructible);
static_assert(__reflect(query_is_constructible, trivially_constructible_class_refl));
static_assert(!__reflect(query_is_constructible, trivially_constructible_class_refl, reflexpr(int)));
static_assert(__reflect(query_is_trivially_constructible, trivially_constructible_class_refl));
static_assert(!__reflect(query_is_trivially_constructible, trivially_constructible_class_refl, reflexpr(int)));
static_assert(__reflect(query_is_nothrow_constructible, trivially_constructible_class_refl));
static_assert(!__reflect(query_is_nothrow_constructible, trivially_constructible_class_refl, reflexpr(int)));
static_assert(__reflect(query_is_assignable, trivially_constructible_class_refl, trivially_constructible_class_refl));
static_assert(!__reflect(query_is_assignable, trivially_constructible_class_refl, reflexpr(int)));
static_assert(__reflect(query_is_trivially_assignable, trivially_constructible_class_refl, trivially_constructible_class_refl));
static_assert(!__reflect(query_is_trivially_assignable, trivially_constructible_class_refl, reflexpr(int)));
static_assert(__reflect(query_is_nothrow_assignable, trivially_constructible_class_refl, trivially_constructible_class_refl));
static_assert(!__reflect(query_is_nothrow_assignable, trivially_constructible_class_refl, reflexpr(int)));
static_assert(__reflect(query_is_destructible, trivially_constructible_class_refl));
static_assert(__reflect(query_is_trivially_destructible, trivially_constructible_class_refl));
static_assert(__reflect(query_is_nothrow_destructible, trivially_constructible_class_refl));

struct nothrow_constructible {
  nothrow_constructible() noexcept { }
  ~nothrow_constructible() noexcept { }
  void operator=(const nothrow_constructible &c) noexcept { }
};

constexpr meta::info nothrow_constructible_class_refl = reflexpr(nothrow_constructible);
static_assert(__reflect(query_is_constructible, nothrow_constructible_class_refl));
static_assert(!__reflect(query_is_constructible, nothrow_constructible_class_refl, reflexpr(int)));
static_assert(!__reflect(query_is_trivially_constructible, nothrow_constructible_class_refl));
static_assert(!__reflect(query_is_trivially_constructible, nothrow_constructible_class_refl, reflexpr(int)));
static_assert(__reflect(query_is_nothrow_constructible, nothrow_constructible_class_refl));
static_assert(!__reflect(query_is_nothrow_constructible, nothrow_constructible_class_refl, reflexpr(int)));
static_assert(__reflect(query_is_assignable, nothrow_constructible_class_refl, nothrow_constructible_class_refl));
static_assert(!__reflect(query_is_assignable, nothrow_constructible_class_refl, reflexpr(int)));
static_assert(!__reflect(query_is_trivially_assignable, nothrow_constructible_class_refl, nothrow_constructible_class_refl));
static_assert(!__reflect(query_is_trivially_assignable, nothrow_constructible_class_refl, reflexpr(int)));
static_assert(__reflect(query_is_nothrow_assignable, nothrow_constructible_class_refl, nothrow_constructible_class_refl));
static_assert(!__reflect(query_is_nothrow_assignable, nothrow_constructible_class_refl, reflexpr(int)));
static_assert(__reflect(query_is_destructible, nothrow_constructible_class_refl));
static_assert(!__reflect(query_is_trivially_destructible, nothrow_constructible_class_refl));
static_assert(__reflect(query_is_nothrow_destructible, nothrow_constructible_class_refl));
