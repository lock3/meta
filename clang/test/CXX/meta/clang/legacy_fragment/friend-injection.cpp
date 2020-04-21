// RUN: %clang_cc1 -freflection -Wno-deprecated-fragment -std=c++2a %s

class Bar;

class Foo {
  int x = 10;

  consteval -> __fragment class Z {
    friend class Bar;

    friend int do_thing(const Z& f);
    friend int do_other_thing(const Z& f) {
      return f.x;
    }
  };
};

class Bar {
  int get_x(const Foo& f) {
    return f.x;
  }
};

int do_thing(const Foo& f) {
  return f.x;
}

int main() {
  return 0;
}
