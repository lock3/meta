// RUN: %clang_cc1 -freflection -std=c++1z %s

#define assert(E) if (!(E)) __builtin_abort();

template<typename T>
constexpr void interface(T source) {
  int default_val = 1;
  -> __fragment struct {
    int val = default_val;

    void set_foo(const int& val) {
      this->val = val;
    }

    int foo() {
      return val;
    }

    void reset_foo() {
      set_foo(default_val);
    }
  };

  -> __fragment struct {
    int dedicated_field;
  };
};

template<typename T>
class(interface) Thing {
};

int main() {
  Thing<int> thing;

  assert(thing.foo() == 1);

  thing.set_foo(2);
  assert(thing.foo() == 2);

  thing.reset_foo();
  assert(thing.foo() == 1);

  return 0;
}
