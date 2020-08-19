// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

union UnionType {
};

static_assert(!__reflect(query_is_class, reflexpr(UnionType)));
static_assert(__reflect(query_is_union, reflexpr(UnionType)));
static_assert(!__reflect(query_has_virtual_destructor, reflexpr(UnionType)));
static_assert(!__reflect(query_is_declared_class, reflexpr(UnionType)));
static_assert(!__reflect(query_is_declared_struct, reflexpr(UnionType)));

struct NonVirtStruct {
};

static_assert(__reflect(query_is_class, reflexpr(NonVirtStruct)));
static_assert(!__reflect(query_is_union, reflexpr(NonVirtStruct)));
static_assert(!__reflect(query_has_virtual_destructor, reflexpr(NonVirtStruct)));
static_assert(!__reflect(query_is_declared_class, reflexpr(NonVirtStruct)));
static_assert(__reflect(query_is_declared_struct, reflexpr(NonVirtStruct)));

class NonVirt {
};

static_assert(__reflect(query_is_class, reflexpr(NonVirt)));
static_assert(!__reflect(query_is_union, reflexpr(NonVirt)));
static_assert(!__reflect(query_has_virtual_destructor, reflexpr(NonVirt)));
static_assert(__reflect(query_is_declared_class, reflexpr(NonVirt)));
static_assert(!__reflect(query_is_declared_struct, reflexpr(NonVirt)));

class NonVirtChild : NonVirt {
};

static_assert(__reflect(query_is_class, reflexpr(NonVirtChild)));
static_assert(!__reflect(query_is_union, reflexpr(NonVirtChild)));
static_assert(!__reflect(query_has_virtual_destructor, reflexpr(NonVirtChild)));
static_assert(__reflect(query_is_declared_class, reflexpr(NonVirtChild)));
static_assert(!__reflect(query_is_declared_struct, reflexpr(NonVirtChild)));

class Virt {
public:
  virtual ~Virt() { }
};

static_assert(__reflect(query_is_class, reflexpr(Virt)));
static_assert(!__reflect(query_is_union, reflexpr(Virt)));
static_assert(__reflect(query_has_virtual_destructor, reflexpr(Virt)));
static_assert(__reflect(query_is_declared_class, reflexpr(Virt)));
static_assert(!__reflect(query_is_declared_struct, reflexpr(Virt)));

class VirtChild : Virt {
};

static_assert(__reflect(query_is_class, reflexpr(VirtChild)));
static_assert(!__reflect(query_is_union, reflexpr(VirtChild)));
static_assert(__reflect(query_has_virtual_destructor, reflexpr(VirtChild)));
static_assert(__reflect(query_is_declared_class, reflexpr(VirtChild)));
static_assert(!__reflect(query_is_declared_struct, reflexpr(VirtChild)));

int x;

static_assert(!__reflect(query_is_class, reflexpr(x)));
static_assert(!__reflect(query_is_union, reflexpr(x)));
