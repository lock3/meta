// RUN: %clang_cc1 -std=c++2a -freflection -verify %s
// expected-no-diagnostics

namespace inline_members {

class container_type {
public:
  int [# "data_member_", 1 #];
  void [# "member_fn_", 1 #]() { }

  static void [# "static_member_fn_", 1 #]() { }
};

void test() {
  container_type container;
  int member_val_1 = container.data_member_1;
  container.member_fn_1();

  container_type::static_member_fn_1();
}

} // end namespace inline_members

namespace dependent_inline_members {

template<int T>
class container_type {
public:
  int [# "data_member_", T #];
  void [# "member_fn_", T #]() { }

  static void [# "static_member_fn_", T #]() { }
};

void test() {
  container_type<1> container;
  int member_val_1 = container.data_member_1;
  container.member_fn_1();

  container_type<1>::static_member_fn_1();
}

} // end namespace dependent_inline_members

namespace outofline_members {

class container_type {
public:
  void [# "member_fn_", 1 #]();

  static int [# "static_data_member_", 1 #];
  static void [# "static_member_fn_", 1 #]();
};

void container_type::[# "member_fn_", 1 #]() { }

int container_type::[# "static_data_member_", 1 #] = 0;
void container_type::[# "static_member_fn_", 1 #]() { }

void test() {
  container_type container;
  container.member_fn_1();

  int static_member_val_1 = container_type::static_data_member_1;
  container_type::static_member_fn_1();
}

} // end namespace outofline_members

namespace dependent_outofline_members {

template<int T>
class container_type {
public:
  void [# "member_fn_", T #]();

  static int [# "static_data_member_", T #];
  static void [# "static_member_fn_", T #]();
};

template<int T>
void container_type<T>::[# "member_fn_", T #]() { }

template<int T>
int container_type<T>::[# "static_data_member_", T #] = 0;
template<int T>
void container_type<T>::[# "static_member_fn_", T #]() { }

void test() {
  container_type<1> container;
  container.member_fn_1();

  int static_member_val_1 = container_type<1>::static_data_member_1;
  container_type<1>::static_member_fn_1();
}

} // end namespace dependent_outofline_members

