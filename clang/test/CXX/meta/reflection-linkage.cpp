// RUN: %clang_cc1 -std=c++2a -freflection -emit-llvm -triple %itanium_abi_triple -o - %s | FileCheck %s
// RUN: %clang_cc1 -std=c++2a -freflection -debug-info-kind=limited -emit-llvm -triple %itanium_abi_triple -o - %s | FileCheck %s

#include "reflection_query.h"

namespace meta {
  using info = decltype(^void);
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
  // CHECK: define linkonce_odr i32 @_Z3fooIReTy7PubTypeEDav(
  foo<^PubType>();
}

void test2() {
  // CHECK: define internal i32 @_Z3fooIReTyN12_GLOBAL__N_18PrivTypeEEDav(
  foo<^PrivType>();
}

void test3() {
  // CHECK: define linkonce_odr i32 @_Z3fooIReTy3barILi1EEEDav(
  foo<^bar<1>>();
}

}

namespace template_reflection {

void test3() {
  // CHECK: define linkonce_odr i32 @_Z3fooIRe3barEDav(
  foo<^bar>();
}

}

namespace namespace_reflection {

void test1() {
  // CHECK: define internal i32 @_Z3fooIReTuEDav(
  foo<^::>();
}

void test2() {
  // CHECK: define linkonce_odr i32 @_Z3fooIRe20namespace_reflectionEDav(
  foo<^::namespace_reflection>();
}

}

namespace expr_reflection {

void test1() {
  // CHECK: define internal i32 @_Z3fooIRe{{[0-9]+}}EDav(
  foo<^(1 + 2)>();
}

}

namespace fragment_reflection {

void test1() {
  // CHECK: define internal i32 @_Z3fooIReF{{[0-9]+}}EDav(
  foo<fragment class { int k; }>();
}

void test2() {

  // CHECK: define internal i32 @_Z3fooIReF{{[0-9]+}}CL_ZZN19fragment_reflection5test2EvE7var_capEEDav(
  constexpr int var_cap = 344;
  foo<fragment class { int k = %{var_cap}; }>();
}

}

