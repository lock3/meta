// RUN: %clang -freflection -Wno-deprecated-fragment -std=c++2a %s

class bar {
};

template<typename T>
struct test {
  consteval -> __fragment struct {
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
