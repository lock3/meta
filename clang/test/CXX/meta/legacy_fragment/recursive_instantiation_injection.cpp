// RUN: %clang -freflection -Wno-deprecated-fragment -std=c++2a %s

template<typename T>
class recursively_instantiated {
  consteval {
    -> __fragment struct {
      T get_t() {
        return { };
      }
    };
  }
};

template<typename T>
class caller {
  consteval {
    -> __fragment struct {
      T get_t() {
        return recursively_instantiated<T>().get_t();
      }
    };
  }
};

int main() {
  caller<int> c;
  return c.get_t();
}

