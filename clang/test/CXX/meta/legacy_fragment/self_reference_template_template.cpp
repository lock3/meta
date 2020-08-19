// RUN: %clang_cc1 -freflection -Wno-deprecated-fragment -std=c++2a %s

template<typename T>
struct templated_holder {
  T value;
};

template<typename Q, template<typename> typename R>
struct test {
  consteval {
    -> __fragment struct {
      R<Q> value_proxy;

      test<Q, R> make() {
        return { };
      }

      template<typename V, template<typename> typename L>
      test<V, L> make_templ() {
        return test<V, L>();
      }
    };
  };
};

int main() {
  test<int, templated_holder> int_test;

  auto default_make_result = int_test.make();
  auto template_make_result = int_test.make_templ<float, templated_holder>();

  return 0;
}

