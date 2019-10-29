// RUN: %clang_cc1 -std=c++2a -freflection -emit-llvm -triple %itanium_abi_triple -o - %s | FileCheck %s

#include "reflection_query.h"

namespace meta {
  using info = decltype(reflexpr(void));
}

template<meta::info refl>
auto do_the_thing() {
  return 0;
}

void test1() {
  // CHECK: define internal i32 @_Z12do_the_thingIXRe{{[0-9]+}}EEDav(
  do_the_thing<__invalid_reflection("Abc")>();
}

namespace {
  struct PrivType {
  };
}

void test2() {
  // CHECK: define internal i32 @_Z12do_the_thingIXReTyN12_GLOBAL__N_18PrivTypeEEEDav(
  do_the_thing<reflexpr(PrivType)>();
}

void test3() {
  // CHECK: define internal i32 @_Z12do_the_thingIXReN12_GLOBAL__N_18PrivTypeEEEDav(
  do_the_thing<__reflect(query_get_definition, reflexpr(PrivType))>();
}

struct PubType {
};

void test4() {
  // CHECK: define linkonce_odr i32 @_Z12do_the_thingIXReTy7PubTypeEEDav(
  do_the_thing<reflexpr(PubType)>();
}

void test5() {
  // CHECK: define linkonce_odr i32 @_Z12do_the_thingIXRe7PubTypeEEDav(
  do_the_thing<__reflect(query_get_definition, reflexpr(PubType))>();
}

namespace foo {
  template<typename T>
  void tmpl() { }
}

void test6() {
  // CHECK: define linkonce_odr i32 @_Z12do_the_thingIXReN3foo4tmplEEEDav(
  do_the_thing<__reflect(query_get_begin, reflexpr(foo))>();
}

void test7() {
  // CHECK: define linkonce_odr i32 @_Z12do_the_thingIXRe3fooEEDav(
  do_the_thing<reflexpr(foo)>();
}

void test8() {
  // CHECK: define internal i32 @_Z12do_the_thingIXRe{{[0-9]+}}EEDav(
  do_the_thing<reflexpr(1 + 2)>();
}

// FIXME: Add test for base specifier.
