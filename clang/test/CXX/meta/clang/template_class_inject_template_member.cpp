// RUN: %clang -freflection -std=c++2a %s

class bar {
};

template<typename T>
struct test {
  consteval -> fragment struct {
    template<typename F>
    F get() {
      return F { };
    }
  };
};

int main() {
  test<bar> gen;
  return gen.get<int>();
}
