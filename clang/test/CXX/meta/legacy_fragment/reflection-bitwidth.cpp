union S {
  consteval -> __fragment union {
    unsigned ac : 4;
    unsigned : 4;
    unsigned clock : 1;
    unsigned : 0;
    unsigned flag : 1;
  };
};

struct X {
  consteval -> __fragment class {
    unsigned light : 1;
    unsigned toaster : 1;
    int count;
    union S stat;
  };
};

// RUN: c-index-test -test-print-bitwidth -freflection -Wno-deprecated-fragment -std=c++2a %s | FileCheck %s
// CHECK: FieldDecl=ac:3:14 (Definition) bitwidth=4
// CHECK: FieldDecl=:4:14 (Definition) bitwidth=4
// CHECK: FieldDecl=clock:5:14 (Definition) bitwidth=1
// CHECK: FieldDecl=:6:14 (Definition) bitwidth=0
// CHECK: FieldDecl=flag:7:14 (Definition) bitwidth=1
// CHECK: FieldDecl=light:13:14 (Definition) bitwidth=1
// CHECK: FieldDecl=toaster:14:14 (Definition) bitwidth=1
// CHECK-NOT: count
// CHECK-NOT: stat
