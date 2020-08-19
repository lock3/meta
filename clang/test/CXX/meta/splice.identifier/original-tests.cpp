// RUN: %clang_cc1 -std=c++2a -freflection -verify %s

namespace bar {
  namespace fin {
  }
}

using namespace bar::fin;

template<typename T>
void test_template() {
  constexpr auto int_reflexpr = reflexpr(int);
  T unqualid("foo_", reflexpr(bar), "_", reflexpr(bar::fin)) = T();

  int int_x = foo_bar_fin.unqualid("get_", unqualid("int_reflexpr"))();
  int int_y = unqualid("foo_bar_fin").get_int();
  int int_z = unqualid("foo_bar_fin").unqualid("get_int")();

  static constexpr auto r = reflexpr(T::field_1);
  static_assert(T::unqualid(r) == 0);
}

template<typename T>
struct TemplateS {
  T unqualid("val_", reflexpr(T));
  T unqualid("get_", reflexpr(T))() { return T(); }
};

template<typename T>
void test_template_class_attribute() {
  TemplateS<T> s;
  T res_val = s.unqualid("val_", reflexpr(T));
  T res_get = s.unqualid("get_", reflexpr(T))();
}

template<int y>
constexpr int get_ret_value(int unqualid("parm_", y)) {
  return unqualid("parm_", y);
}

void test_parameter() {
  static_assert(get_ret_value<1>(3) == 3);
}

struct SBase {
  static constexpr int base_field_1 = 90;

  constexpr int base_method_1() const {
    return 80;
  }
};

struct S : public SBase {
  constexpr S() { }

  int get_int() { return 0; }

  static constexpr int field_1 = 0;

  template<int y>
  constexpr int get_template_field_int() const {
    return unqualid("field_", y);
  }

  template<int y>
  constexpr int get_template_base_field_int() const {
    return unqualid("base_field_", y);
  }

  constexpr int method_1() const {
    return 0;
  }

  template<int y>
  constexpr int get_template_method_int() const {
    return unqualid("method_", y)();
  }

  template<int y>
  constexpr int get_template_base_method_int() const {
    return unqualid("base_method_", y)();
  }
};

void test_non_template() {
  constexpr auto int_reflexpr = reflexpr(int);
  S unqualid("foo_", reflexpr(bar), "_", reflexpr(bar::fin)) = S();

  int int_x = foo_bar_fin.unqualid("get_", unqualid("int_reflexpr"))();
  int int_y = unqualid("foo_bar_fin").get_int();
  int int_z = unqualid("foo_bar_fin").unqualid("get_int")();
}

template<int y>
constexpr int template_bad_local_var_function() {
  int z1 = unqualid("x", y); // expected-error {{use of undeclared identifier 'x1'}}
  int x1 = 0;
  return x1;
}

template<int y>
constexpr int template_bad_local_var_in_caller_function() {
  return unqualid("local_var_", y); // expected-error {{use of undeclared identifier 'local_var_1'}}
}

template<int y>
constexpr int template_bad_local_var_in_caller_calling_function() {
  int local_var_1 = 0;
  return template_bad_local_var_in_caller_function<y>(); // expected-note {{in instantiation of function template specialization 'template_bad_local_var_in_caller_function<1>' requested here}}
}

void test_bad() {
  auto not_a_reflexpr = 1;

  S foo_bar_fin  = S();
  int int_x = foo_bar_fin.unqualid("get_nothing")(); // expected-error {{no member named 'get_nothing' in 'S'}}
  int int_y = foo_bar_fin.unqualid("get_", unqualid("not_a_reflexpr"))(); // expected-error {{expression is not an integral constant expression}} expected-error {{no member named '__invalid_identifier_splice' in 'S'}}
  template_bad_local_var_function<1>(); // expected-note {{in instantiation of function template specialization 'template_bad_local_var_function<1>' requested here}}
  template_bad_local_var_in_caller_calling_function<1>(); // expected-note {{in instantiation of function template specialization 'template_bad_local_var_in_caller_calling_function<1>' requested here}}
}

constexpr void function_foo_1() { }

template<int y>
constexpr bool template_function_address_check() {
  return &unqualid("function_foo_", y) == &function_foo_1;
}

template<int y>
constexpr int template_parm_function(int x1) {
  return unqualid("x", y);
}

template<int y>
constexpr int template_local_var_function() {
  int x1 = 2;
  return unqualid("x", y);
}

constexpr int global_var_1 = 1;

template<int y>
constexpr int template_global_var_function() {
  return unqualid("global_var_", y);
}

template<int y>
constexpr int template_proxy_function_1() {
  return template_global_var_function<y>();
}

template<int y>
constexpr int template_template_non_dependent_unqualid_call_function() {
  return template unqualid("template_proxy_function_", 1)<y>();
}

template<int y>
constexpr int template_template_non_dependent_implicit_unqualid_call_function() {
  return unqualid("template_proxy_function_", 1)<y>();
}

template<int y>
constexpr int template_template_unqualid_call_function() {
  return template unqualid("template_proxy_function_", y)<y>();
}

namespace namespace_a {
  struct NamespaceAStruct {
    int value = 22;
  };

  constexpr int namespace_var_1 = 12;

  constexpr int adl_found_function_1(const NamespaceAStruct &x) {
    return x.value;
  }

  template<int y>
  constexpr int template_namespace_var_function() {
    return unqualid("namespace_var_", y);
  }
}

template<int y>
constexpr int template_adl_function() {
  namespace_a::NamespaceAStruct arg;
  return unqualid("adl_found_function_", y)(arg);
}

template<int y>
constexpr int template_other_namespace_var_function() {
  return namespace_a::unqualid("namespace_var_", y);
}

template<int y>
struct template_struct {
  int field_1 = 73;
  int field_2 = unqualid("field_", y);
};

void test_template_param() {
  static_assert(template_function_address_check<1>());

  static_assert(template_parm_function<1>(3) == 3);
  static_assert(template_local_var_function<1>() == 2);
  static_assert(template_global_var_function<1>() == 1);
  static_assert(template_template_non_dependent_unqualid_call_function<1>() == 1);
  static_assert(template_template_non_dependent_implicit_unqualid_call_function<1>() == 1);
  static_assert(template_template_unqualid_call_function<1>() == 1);
  static_assert(namespace_a::template_namespace_var_function<1>() == 12);
  static_assert(template_adl_function<1>() == 22);
  static_assert(template_other_namespace_var_function<1>() == 12);

  constexpr S struct_s;
  static_assert(struct_s.get_template_field_int<1>() == 0);
  static_assert(struct_s.get_template_base_field_int<1>() == 90);
  static_assert(struct_s.get_template_method_int<1>() == 0);
  static_assert(struct_s.get_template_base_method_int<1>() == 80);

  static_assert(template_struct<1>().field_2 == 73);
}

int main() {
  test_template<S>();
  test_template_class_attribute<int>();
  test_template_class_attribute<S>();
  test_parameter();
  test_non_template();
  test_bad();
  test_template_param();
  return 0;
}
