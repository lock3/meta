// RUN: %clang_cc1 -freflection -std=c++1z %s

namespace meta {
  using info = decltype(reflexpr(void));
}

consteval void test_metaclass(meta::info source) {
  -> __fragment struct {
    template<typename T> struct MemberClassTemplate {
      T t_val;
    };

    template<>
    struct MemberClassTemplate<float> {
      float float_val;
    };

    template<int Y> struct MemberClassValTemplate {
      constexpr int get_t_val() { return Y; }
    };
  };

  -> __fragment struct {
    int x = 0;
  };
}

class(test_metaclass) test_class {
};

int main() {
  test_class::MemberClassTemplate<int> metaclass_nested_class_int;
  metaclass_nested_class_int.t_val = 1;

  test_class::MemberClassTemplate<float> metaclass_nested_class_float;
  metaclass_nested_class_float.float_val = 1;

  test_class::MemberClassValTemplate<1> metaclass_nested_class_val;
  static_assert(metaclass_nested_class_val.get_t_val() == 1);

  return 0;
}
