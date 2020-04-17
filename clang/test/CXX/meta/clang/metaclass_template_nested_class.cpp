// RUN: %clang_cc1 -freflection -std=c++2a %s

namespace meta {
  using info = decltype(reflexpr(void));
}

consteval auto generate_func() {
  return fragment struct S {
    int int_val = 12;

    constexpr S() = default;

    consteval int get_int() const {
      return 24;
    }
  };
}

consteval void test_metaclass(meta::info source) {
  -> fragment struct {
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

    template<template<typename> class H, typename S>
    struct MemberClassTemplateTemplate {
      H<S> template_val;
    };

    template<typename T>
    class MemberClassTemplateWithMetaprogram {
      consteval -> generate_func();
    };
  };

  -> fragment struct {
    int x = 0;
  };
}

class(test_metaclass) test_class {
};

template<typename T>
struct Container {
  constexpr T get_val() { return 1; }
};

int main() {
  using MCTIntTy = test_class::MemberClassTemplate<int>;
  MCTIntTy metaclass_nested_class_int;
  metaclass_nested_class_int.t_val = 1;

  using MCTFloatTy = test_class::MemberClassTemplate<float>;
  MCTFloatTy metaclass_nested_class_float;
  metaclass_nested_class_float.float_val = 1;

  using MCVTy = test_class::MemberClassValTemplate<1>;
  MCVTy metaclass_nested_class_val;
  static_assert(metaclass_nested_class_val.get_t_val() == 1);

  using MCTTTy = test_class::MemberClassTemplateTemplate<Container, int>;
  MCTTTy metaclass_nested_template_template;
  static_assert(metaclass_nested_template_template.template_val.get_val() == 1);

  using MCTWMTy = test_class::MemberClassTemplateWithMetaprogram<int>;
  constexpr MCTWMTy metaclass_nested_class_metaprogram;
  static_assert(metaclass_nested_class_metaprogram.int_val == 12);
  static_assert(metaclass_nested_class_metaprogram.get_int() == 24);

  return 0;
}
