// RUN: %clang_cc1 -freflection -Wno-deprecated-fragment -std=c++2a %s

template<int x>
consteval void inject() {
  -> __fragment struct {
    int get() {
      return x;
    }
  };
}

template<int x>
class nested_foo {
  consteval {
    inject<x>();
  }
};

class foo {
  consteval {
    int i = 0;
    -> __fragment struct {
      int unqualid("get_value_", i)() {
        return nested_foo<i> { }.get();
      }
    };
  };
};

int main() {
  foo f;
  return f.get_value_0();
}
