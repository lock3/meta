
struct S { };

namespace N { }

static_assert(reflexpr(N) == reflexpr(N));
static_assert(reflexpr(N) != reflexpr(S));
static_assert(reflexpr(int) == reflexpr(int));
static_assert(reflexpr(int) != reflexpr(const int));

static_assert(reflexpr(int) < reflexpr(int)); // expected-error {{invalid operands}}
static_assert(reflexpr(int) > reflexpr(int)); // expected-error {{invalid operands}}
static_assert(reflexpr(int) <= reflexpr(int)); // expected-error {{invalid operands}}
static_assert(reflexpr(int) >= reflexpr(int)); // expected-error {{invalid operands}}
static_assert(reflexpr(int) <=> reflexpr(int)); // expected-error {{invalid operands}}
