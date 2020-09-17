// RUN: %clang_cc1 -std=c++2a -freflection -emit-llvm -triple %itanium_abi_triple -o - %s | FileCheck %s
// RUN: %clang_cc1 -std=c++2a -freflection -debug-info-kind=limited -emit-llvm -triple %itanium_abi_triple -o - %s | FileCheck %s

#include "reflection_query.h"

namespace meta {
  using info = decltype(reflexpr(void));
}

template<meta::info refl>
auto foo() {
  return 0;
}

struct PubType {
};

namespace {
  struct PrivType {
  };
}

template<int x>
class bar {
};

namespace type_reflection {

void test1() {
  // CHECK: define linkonce_odr i32 @_Z3fooIXReTy7PubTypeEEDav(
  foo<reflexpr(PubType)>();
}

void test2() {
  // CHECK: define internal i32 @_Z3fooIXReTyN12_GLOBAL__N_18PrivTypeEEEDav(
  foo<reflexpr(PrivType)>();
}

void test3() {
  // CHECK: define linkonce_odr i32 @_Z3fooIXReTy3barILi1EEEEDav(
  foo<reflexpr(bar<1>)>();
}

}

namespace template_reflection {

void test3() {
  // CHECK: define linkonce_odr i32 @_Z3fooIXRe3barEEDav(
  foo<reflexpr(bar)>();
}

}

namespace namespace_reflection {

void test1() {
  // CHECK: define internal i32 @_Z3fooIXReTuEEDav(
  foo<reflexpr(::)>();
}

void test2() {
  // CHECK: define linkonce_odr i32 @_Z3fooIXRe20namespace_reflectionEEDav(
  foo<reflexpr(::namespace_reflection)>();
}

}

namespace expr_reflection {

void test1() {
  // CHECK: define internal i32 @_Z3fooIXRe{{[0-9]+}}EEDav(
  foo<reflexpr(1 + 2)>();
}

}
