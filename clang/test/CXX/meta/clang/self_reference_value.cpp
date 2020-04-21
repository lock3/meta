// RUN: %clang_cc1 -freflection -std=c++2a %s

template<int Q>
struct test {
  consteval {
    -> fragment struct {
      test<Q> make() {
        return { };
      }

      template<int V>
      test<V> make_templ() {
        return test<V>();
      }
    };
  };
};

int main() {
  test<1> int_test;

  auto default_make_result = int_test.make();
  auto template_make_result = int_test.make_templ<2>();

  return 0;
}

