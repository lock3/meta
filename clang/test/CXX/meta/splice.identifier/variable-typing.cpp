// RUN: %clang_cc1 -std=c++2a -freflection -verify %s

namespace local_var {

class A1 {
};

template<int I>
class B1 {
};

void C1() { }

void test() {
  [# "A", 1 #] a;
  local_var::[# "A", 1 #] qualified_a;
  [# "B", 1 #]<1> b;
  local_var::[# "B", 1 #]<1> qualified_b;
  [# "C", 1 #] c;
  // expected-error@-1 {{expected ';' after expression}}
  // expected-warning@-2 {{expression result unused}}
  // expected-error@-3 {{use of undeclared identifier 'c'}}
  local_var::[# "C", 1 #] qualified_c;
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
  typename [# "A", 1 #] a;
  typename kw_local_var::[# "A", 1 #] qualified_a;
  typename [# "B", 1 #]<1> b;
  typename kw_local_var::[# "B", 1 #]<1> qualified_b;
  typename [# "C", 1 #] c; // expected-error {{identifier splice does not name a type or class template}}
  typename kw_local_var::[# "C", 1 #] qualified_c; // expected-error {{identifier splice does not name a type or class template}}
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
  typename [# "A", I #] a;
  typename dependent_local_var::[# "A", I #] qualified_a;
  typename [# "B", I #]<I> b;
  typename dependent_local_var::[# "B", I #]<I> qualified_b;
}

// Note these test cases are split as the first failure stops further
// DeclStmts from being processed.

template<int I>
void foo_failure_a() {
  typename [# "C", I #] c; // expected-error {{identifier splice does not name a type or class template}}
}

template<int I>
void foo_failure_b() {
  typename dependent_local_var::[# "C", I #] qualified_c; // expected-error {{identifier splice does not name a type or class template}}
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
  typename hidden::[# "A", I #] a;
  typename hidden::[# "B", I #]<I> b;
  typename hidden::[# "C", I #] c; // expected-error {{identifier splice does not name a type or class template}}
}

void test() {
  foo<1>(); // expected-note {{in instantiation of function template specialization 'hidden_dependent_local_var::foo<1>' requested here}}
}

} // end namespace hidden_dependent_local_var

namespace inline_class_var {

class {} [# "a" #];

} // end namespace class_var

namespace dependent_inline_class_var {

template<int I>
struct A1 {
  class {} [# "a" , I #];
};

void test() {
  A1<1>();
}

} // end namespace depedent_inline_class_var
