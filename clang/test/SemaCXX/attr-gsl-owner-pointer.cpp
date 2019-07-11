// RUN: %clang_cc1 -fsyntax-only -verify %s

int [[gsl::Owner]] i;
// expected-error@-1 {{'Owner' attribute cannot be applied to types}}
void [[gsl::Owner]] f();
// expected-error@-1 {{'Owner' attribute cannot be applied to types}}

[[gsl::Owner]] void f();
// expected-warning@-1 {{'Owner' attribute only applies to classes}}

struct S {
};
static_assert(!__is_gsl_owner(S), "");
static_assert(!__is_gsl_pointer(S), "");

S [[gsl::Owner]] Instance;
// expected-error@-1 {{'Owner' attribute cannot be applied to types}}

class [[gsl::Owner]] OwnerMissingParameter{};

class [[gsl::Pointer]] PointerMissingParameter{};

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
static_assert(__is_gsl_owner(DuplicateOwner), "");
static_assert(!__is_gsl_pointer(DuplicateOwner), "");

class [[gsl::Pointer(int)]] [[gsl::Pointer(int)]] DuplicatePointer{};
static_assert(!__is_gsl_owner(DuplicatePointer), "");
static_assert(__is_gsl_pointer(DuplicatePointer), "");

class [[gsl::Owner(void)]] OwnerVoidDerefType{};
// expected-error@-1 {{'void' is an invalid argument to attribute 'Owner'}}
class [[gsl::Pointer(void)]] PointerVoidDerefType{};
// expected-error@-1 {{'void' is an invalid argument to attribute 'Pointer'}}

class [[gsl::Owner(int)]] AnOwner{};
static_assert(__is_gsl_owner(AnOwner), "");
static_assert(!__is_gsl_pointer(AnOwner), "");

class [[gsl::Pointer(S)]] APointer{};
static_assert(!__is_gsl_owner(APointer), "");
static_assert(__is_gsl_pointer(APointer), "");

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

class [[gsl::Owner()]] OwnerWithEmptyParameterList{};
static_assert(__is_gsl_owner(OwnerWithEmptyParameterList), "");

class [[gsl::Pointer()]] PointerWithEmptyParameterList{};
static_assert(__is_gsl_pointer(PointerWithEmptyParameterList), "");

class [[gsl::Owner()]] [[gsl::Owner(int)]] WithAndWithoutParameter{};
// expected-error@-1 {{'Owner' and 'Owner' attributes are not compatible}}
// expected-note@-2 {{conflicting attribute is here}}

static_assert(!__is_gsl_pointer(int), "");
static_assert(__is_gsl_pointer(int &), "");
static_assert(__is_gsl_pointer(int *), "");

// Test builtin attributes for std types.
namespace std {
// Attributes are added to a (complete) class.
class any {
};
static_assert(__is_gsl_owner(any), "");

// Attributes are added to a instantiatons of a complete template.
template <typename T>
class vector {
public:
  class iterator {};
};
static_assert(__is_gsl_owner(vector<int>), "");
static_assert(__is_gsl_pointer(vector<int>::iterator), "");

// If std::container::iterator is a using declaration, Attributes are added to
// the underlying class
template <typename T>
class __set_iterator {};

template <typename T>
class set {
public:
  using iterator = __set_iterator<T>;
};
static_assert(__is_gsl_pointer(set<int>::iterator), "");

// If std::container::iterator is a typedef, Attributes are added to the
// underlying class. Inline namespaces are ignored when checking if
// the class lives in the std namespace.
inline namespace inlinens {
template <typename T>
class __map_iterator {};

template <typename T>
class map {
public:
  typedef __map_iterator<T> iterator;
};
} // namespace inlinens
static_assert(__is_gsl_pointer(map<int>::iterator), "");

// std::list has an implicit gsl::Owner attribute,
// but explicit attributes take precedence.
template <typename T>
class [[gsl::Pointer]] list{};
static_assert(!__is_gsl_owner(list<int>), "");
static_assert(__is_gsl_pointer(list<int>), "");

// Forward declared template (Owner)
template <
    class CharT,
    class Traits>
class basic_regex;
static_assert(__is_gsl_owner(basic_regex<char, void>), "");

// Forward declared template (Pointer)
template <class T>
class reference_wrapper;
static_assert(__is_gsl_pointer(reference_wrapper<char>), "");

class some_unknown_type;
static_assert(!__is_gsl_pointer(some_unknown_type), "");
static_assert(!__is_gsl_owner(some_unknown_type), "");
} // namespace std

namespace user {
// If a class is not in the std namespace, we don't add implicit attributes.
class any {
};
static_assert(!__is_gsl_owner(any), "");
} // namespace user
