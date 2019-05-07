// RUN: %clang_cc1 -freflection -std=c++1z %s

class Bar;

class Foo {
  int x = 10;

  consteval -> __fragment class Z {
    friend class Bar;
    friend int do_thing(const Z& f);
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
