// RUN: %clang_cc1 -fsyntax-only -verify %s

int [[gsl::Owner]] i;
// expected-error@-1 {{'Owner' attribute cannot be applied to types}}
void [[gsl::Owner]] f();
// expected-error@-1 {{'Owner' attribute cannot be applied to types}}

[[gsl::Owner]] void f();
// expected-warning@-1 {{'Owner' attribute only applies to classes}}

struct S {
};

S [[gsl::Owner]] Instance;
// expected-error@-1 {{'Owner' attribute cannot be applied to types}}

class [[gsl::Owner]] OwnerMissingParameter{};
// expected-error@-1 {{'Owner' attribute takes one argument}}
class [[gsl::Pointer]] PointerMissingParameter{};
// expected-error@-1 {{'Pointer' attribute takes one argument}}

class [[gsl::Owner(7)]] OwnerDerefNoType{};
// expected-error@-1 {{expected a type}} expected-error@-1 {{expected ')'}}
// expected-note@-2 {{to match this '('}}

class [[gsl::Pointer("int")]] PointerDerefNoType{};
// expected-error@-1 {{expected a type}} expected-error@-1 {{expected ')'}}
// expected-note@-2 {{to match this '('}}

class [[gsl::Owner(int)]] [[gsl::Pointer(int)]] BothOwnerPointer{};
// expected-error@-1 {{'Pointer' and 'Owner' attributes are not compatible}}
// expected-note@-2 {{conflicting attribute is here}}

class [[gsl::Owner(int)]] [[gsl::Owner(int)]] DuplicateOwner{};

class [[gsl::Pointer(int)]] [[gsl::Pointer(int)]] DuplicatePointer{};

class [[gsl::Owner(void)]] OwnerVoidDerefType{};
// expected-error@-1 {{'void' is an invalid argument to attribute 'Owner'}}
class [[gsl::Pointer(void)]] PointerVoidDerefType{};
// expected-error@-1 {{'void' is an invalid argument to attribute 'Pointer'}}

class [[gsl::Owner(int)]] AnOwner{};
class [[gsl::Pointer(S)]] APointer{};

class AddOwnerLater {};
class [[gsl::Owner(int)]] AddOwnerLater;

class [[gsl::Pointer(int)]] AddConflictLater{};
class [[gsl::Owner(int)]] AddConflictLater;
// expected-error@-1 {{'Owner' and 'Pointer' attributes are not compatible}}
// expected-note@-3 {{conflicting attribute is here}}

class [[gsl::Owner(int)]] AddConflictLater2{};
class [[gsl::Owner(float)]] AddConflictLater2;
// expected-error@-1 {{'Owner' and 'Owner' attributes are not compatible}}
// expected-note@-3 {{conflicting attribute is here}}

class [[gsl::Owner(int)]] AddTheSameLater{};
class [[gsl::Owner(int)]] AddTheSameLater;
