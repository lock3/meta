// RUN: %clang_cc1 -freflection -Wno-deprecated-fragment -std=c++2a %s

template<int T>
class foo {
  consteval {
    for (int i = 0; i < 2; ++i) {
      -> __fragment struct {
        void unqualid("func_", T + i) () {
        }
      };
    }
  }
};

int main() {
  foo<1> f;
  return 0;
}
