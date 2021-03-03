// RUN: %clang_cc1 -std=c++2a -freflection -emit-llvm -triple %itanium_abi_triple -o - %s | FileCheck %s

template<typename T>
auto get_typename_t(T t) -> typename [:^T:] { return T(); }

void test1() {
  get_typename_t(1);
}

// Would normally be defined as:
//
// define {{.*}} @_Z5test1v()
//    call i32 @_Z14get_typename_tIiET_S0_(i32 1)
//
// Check for slightly different reflection mangling:
//
// CHECK: define {{.*}} @_Z5test1v()
// CHECK:    call i32 @_Z14get_typename_tIiERTReTyT_ES0_(i32 1)


// CHECK: define dso_local i32 @_Z7get_numv() #0 {
typename [: ^int :] get_num() { return 1; }

struct Foo {
  int x = 0;
  int y = 0;
};

// CHECK: define linkonce_odr void @_ZN3FooC2Ev(%struct.Foo* nonnull dereferenceable(8) %this)
typename [: ^Foo :] get_foo() { return Foo(); }
