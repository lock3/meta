// RUN: %clang -freflection -std=c++2a %s

template<typename T>
class recursively_instantiated {
  consteval {
    -> fragment struct {
      T get_t() {
        return { };
      }
    };
  }
};

template<typename T>
class caller {
  consteval {
    -> fragment struct {
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

