// RUN: %clang_cc1 -freflection -Wno-deprecated-fragment -std=c++2a %s

template<typename Q>
struct test {
  consteval {
    -> __fragment struct {
      test<Q> make() {
        return { };
      }

      template<typename V>
      test<V> make_templ() {
        return test<V>();
      }
    };
  };
};

int main() {
  test<int> int_test;

  auto default_make_result = int_test.make();
  auto template_make_result = int_test.make_templ<float>();

  return 0;
}

