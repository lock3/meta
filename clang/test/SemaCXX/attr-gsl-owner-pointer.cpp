// RUN: %clang_cc1 -ast-dump %s | \
// RUN: FileCheck --implicit-check-not OwnerAttr --implicit-check-not PointerAttr %s

class [[gsl::Owner]] OwnerMissingParameter{};
// CHECK: CXXRecordDecl {{.*}} OwnerMissingParameter
// CHECK: OwnerAttr

class [[gsl::Pointer]] PointerMissingParameter{};
// CHECK: CXXRecordDecl {{.*}} PointerMissingParameter
// CHECK: PointerAttr

class [[gsl::Owner()]] OwnerWithEmptyParameterList{};
// CHECK: CXXRecordDecl {{.*}} OwnerWithEmptyParameterList
// CHECK: OwnerAttr {{.*}}

class [[gsl::Pointer()]] PointerWithEmptyParameterList{};
// CHECK: CXXRecordDecl {{.*}} PointerWithEmptyParameterList
// CHECK: PointerAttr {{.*}}

class [[gsl::Owner(int)]] AnOwner{};
// CHECK: CXXRecordDecl {{.*}} AnOwner
// CHECK: OwnerAttr {{.*}} int

struct S;
class [[gsl::Pointer(S)]] APointer{};
// CHECK: CXXRecordDecl {{.*}} APointer
// CHECK: PointerAttr {{.*}} S

class [[gsl::Owner(int)]] [[gsl::Owner(int)]] DuplicateOwner{};
// CHECK: CXXRecordDecl {{.*}} DuplicateOwner
// CHECK: OwnerAttr {{.*}} int

class [[gsl::Pointer(int)]] [[gsl::Pointer(int)]] DuplicatePointer{};
// CHECK: CXXRecordDecl {{.*}} DuplicatePointer
// CHECK: PointerAttr {{.*}} int

class [[gsl::Owner(int)]] AddTheSameLater{};
// CHECK: CXXRecordDecl {{.*}} AddTheSameLater
// CHECK: OwnerAttr {{.*}} int

class [[gsl::Owner(int)]] AddTheSameLater;
// CHECK: CXXRecordDecl {{.*}} prev {{.*}} AddTheSameLater
// CHECK: OwnerAttr {{.*}} int
