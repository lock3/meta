// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

union UnionType {
};

static_assert(!__reflect(query_is_class, ^UnionType));
static_assert(__reflect(query_is_union, ^UnionType));
static_assert(!__reflect(query_has_virtual_destructor, ^UnionType));
static_assert(!__reflect(query_is_declared_class, ^UnionType));
static_assert(!__reflect(query_is_declared_struct, ^UnionType));

struct NonVirtStruct {
};

static_assert(__reflect(query_is_class, ^NonVirtStruct));
static_assert(!__reflect(query_is_union, ^NonVirtStruct));
static_assert(!__reflect(query_has_virtual_destructor, ^NonVirtStruct));
static_assert(!__reflect(query_is_declared_class, ^NonVirtStruct));
static_assert(__reflect(query_is_declared_struct, ^NonVirtStruct));

class NonVirt {
};

static_assert(__reflect(query_is_class, ^NonVirt));
static_assert(!__reflect(query_is_union, ^NonVirt));
static_assert(!__reflect(query_has_virtual_destructor, ^NonVirt));
static_assert(__reflect(query_is_declared_class, ^NonVirt));
static_assert(!__reflect(query_is_declared_struct, ^NonVirt));

class NonVirtChild : NonVirt {
};

static_assert(__reflect(query_is_class, ^NonVirtChild));
static_assert(!__reflect(query_is_union, ^NonVirtChild));
static_assert(!__reflect(query_has_virtual_destructor, ^NonVirtChild));
static_assert(__reflect(query_is_declared_class, ^NonVirtChild));
static_assert(!__reflect(query_is_declared_struct, ^NonVirtChild));

class Virt {
public:
  virtual ~Virt() { }
};

static_assert(__reflect(query_is_class, ^Virt));
static_assert(!__reflect(query_is_union, ^Virt));
static_assert(__reflect(query_has_virtual_destructor, ^Virt));
static_assert(__reflect(query_is_declared_class, ^Virt));
static_assert(!__reflect(query_is_declared_struct, ^Virt));

class VirtChild : Virt {
};

static_assert(__reflect(query_is_class, ^VirtChild));
static_assert(!__reflect(query_is_union, ^VirtChild));
static_assert(__reflect(query_has_virtual_destructor, ^VirtChild));
static_assert(__reflect(query_is_declared_class, ^VirtChild));
static_assert(!__reflect(query_is_declared_struct, ^VirtChild));

int x;

static_assert(!__reflect(query_is_class, ^x));
static_assert(!__reflect(query_is_union, ^x));
