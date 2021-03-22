// RUN: %clang_cc1 -std=c++2a -freflection %s

#include "../../support/query.h"

namespace complete_type {

template<typename T>
class clazz { };

constexpr auto specialization = ^clazz<int>;
static_assert(!__reflect(query_is_invalid, specialization));
static_assert(__reflect(query_is_complete, specialization));
static_assert(__reflect(query_is_implicit_instantiation, specialization));

}

namespace incomplete_type {

template<typename T>
class clazz;

constexpr auto specialization = ^clazz<int>;
static_assert(!__reflect(query_is_invalid, specialization));
static_assert(!__reflect(query_is_complete, specialization));
static_assert(!__reflect(query_is_implicit_instantiation, specialization));

}

