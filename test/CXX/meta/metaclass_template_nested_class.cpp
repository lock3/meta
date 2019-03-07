// RUN: %clang_cc1 -freflection -std=c++1z %s

namespace meta {
  using info = decltype(reflexpr(void));
}

consteval void test_metaclass(meta::info source) {
  -> __fragment struct {
    template<typename T> struct MemberClassTemplate {
      T val;
    };
  };

  -> __fragment struct {
    int x = 0;
  };
}

class(test_metaclass) test_class {
};

int main() {
  test_class::MemberClassTemplate<int> metaclass_nested_class;
  metaclass_nested_class.val = 1;
  return 0;
}
