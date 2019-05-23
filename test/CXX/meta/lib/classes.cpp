// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../reflection_query.h"

class NonVirt {
};

static_assert(__reflect(query_is_class, reflexpr(NonVirt)));
static_assert(!__reflect(query_has_virtual_destructor, reflexpr(NonVirt)));

class NonVirtChild : NonVirt {
};

static_assert(__reflect(query_is_class, reflexpr(NonVirtChild)));
static_assert(!__reflect(query_has_virtual_destructor, reflexpr(NonVirtChild)));

class Virt {
public:
  virtual ~Virt() { }
};

static_assert(__reflect(query_is_class, reflexpr(Virt)));
static_assert(__reflect(query_has_virtual_destructor, reflexpr(Virt)));

class VirtChild : Virt {
};

static_assert(__reflect(query_is_class, reflexpr(VirtChild)));
static_assert(__reflect(query_has_virtual_destructor, reflexpr(VirtChild)));

int x;

static_assert(!__reflect(query_is_class, reflexpr(x)));
