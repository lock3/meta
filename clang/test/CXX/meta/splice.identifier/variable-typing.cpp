// RUN: %clang_cc1 -std=c++2a -freflection -verify %s

namespace local_var {

class A1 {
};

template<int I>
class B1 {
};

void C1() { }

void test() {
  unqualid("A", 1) a;
  local_var::unqualid("A", 1) qualified_a;
  unqualid("B", 1)<1> b;
  local_var::unqualid("B", 1)<1> qualified_b;
  unqualid("C", 1) c;
  // expected-error@-1 {{expected ';' after expression}}
  // expected-warning@-2 {{expression result unused}}
  // expected-error@-3 {{use of undeclared identifier 'c'}}
  local_var::unqualid("C", 1) qualified_c;
  // expected-error@-1 {{expected ';' after expression}}
  // expected-warning@-2 {{expression result unused}}
  // expected-error@-3 {{use of undeclared identifier 'qualified_c'}}
}

} // end namespace local_var

namespace kw_local_var {

class A1 {
};

template<int I>
class B1 {
};

void C1() { } // expected-note {{splice resolved to 'C1' here}} expected-note {{splice resolved to 'C1' here}}

void test() {
  typename unqualid("A", 1) a;
  typename kw_local_var::unqualid("A", 1) qualified_a;
  typename unqualid("B", 1)<1> b;
  typename kw_local_var::unqualid("B", 1)<1> qualified_b;
  typename unqualid("C", 1) c; // expected-error {{identifier splice does not name a type or class template}}
  typename kw_local_var::unqualid("C", 1) qualified_c; // expected-error {{identifier splice does not name a type or class template}}
}

} // end namespace kw_local_var

namespace dependent_local_var {

class A1 {
};

template<int I>
class B1 {
};

void C1() { } // expected-note {{splice resolved to 'C1' here}} expected-note {{splice resolved to 'C1' here}}

template<int I>
void foo() {
  typename unqualid("A", I) a;
  typename dependent_local_var::unqualid("A", I) qualified_a;
  typename unqualid("B", I)<I> b;
  typename dependent_local_var::unqualid("B", I)<I> qualified_b;
}

// Note these test cases are split as the first failure stops further
// DeclStmts from being processed.

template<int I>
void foo_failure_a() {
  typename unqualid("C", I) c; // expected-error {{identifier splice does not name a type or class template}}
}

template<int I>
void foo_failure_b() {
  typename dependent_local_var::unqualid("C", I) qualified_c; // expected-error {{identifier splice does not name a type or class template}}
}

void test() {
  foo<1>();
  foo_failure_a<1>(); // expected-note {{in instantiation of function template specialization 'dependent_local_var::foo_failure_a<1>' requested here}}
  foo_failure_b<1>(); // expected-note {{in instantiation of function template specialization 'dependent_local_var::foo_failure_b<1>' requested here}}
}

} // end namespace dependent_local_var

namespace hidden_dependent_local_var {

namespace hidden {

class A1 {
};

template<int I>
class B1 {
};

void C1() { } // expected-note {{splice resolved to 'C1' here}}

} // end namespace hidden

template<int I>
void foo() {
  typename hidden::unqualid("A", I) a;
  typename hidden::unqualid("B", I)<I> b;
  typename hidden::unqualid("C", I) c; // expected-error {{identifier splice does not name a type or class template}}
}

void test() {
  foo<1>(); // expected-note {{in instantiation of function template specialization 'hidden_dependent_local_var::foo<1>' requested here}}
}

} // end namespace hidden_dependent_local_var

namespace inline_class_var {

class {} unqualid("a");

} // end namespace class_var

namespace dependent_inline_class_var {

template<int I>
struct A1 {
  class {} unqualid("a" , I);
};

void test() {
  A1<1>();
}

} // end namespace depedent_inline_class_var
