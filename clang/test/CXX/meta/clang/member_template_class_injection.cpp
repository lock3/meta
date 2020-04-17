// RUN: %clang_cc1 -freflection -std=c++2a %s

namespace meta {
  using info = decltype(reflexpr(void));
}

consteval void test_metaclass(meta::info source) {
  -> fragment class {
  public:
    template<typename T = char>
    class member_template {
    public:
      member_template() {}
      member_template(int) {}
      member_template(T) {}
    };
  };
}

namespace ns {
  class(test_metaclass) test_class {};
}

int main() {
  ns::test_class::member_template<>();
  ns::test_class::member_template<>(5);
  ns::test_class::member_template<>('C');
  ns::test_class::member_template<char const*>("Hello!");
  return 0;
}
