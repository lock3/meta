// RUN: %clang_cc1 -freflection -std=c++2a %s

#define assert(E) if (!(E)) __builtin_abort();

class ParentA {
public:
  virtual ~ParentA() { }

  virtual int do_thing() const {
    return 0;
  }
};

class ParentB {
public:
  virtual int do_other_thing() const {
    return 0;
  }
};

class Child : public ParentB {
  consteval {
    __inject_base(public ParentA);
  }

  int do_thing() const override {
    return 1;
  }

  int do_other_thing() const override {
    return 1;
  }
};

template<typename T>
class TemplateChild : public ParentB {
  consteval {
    __inject_base(public T);

    -> fragment struct {
      int do_thing() const override {
        return 1;
      }
    };
  }

  int do_other_thing() const override {
    return 1;
  }
};

int do_thing(const ParentA &a) {
  return a.do_thing();
}

int do_other_thing(const ParentB &b) {
  return b.do_other_thing();
}

int main() {
  {
    ParentA a;
    assert(do_thing(a) == 0);
  }
  {
    ParentB b;
    assert(do_other_thing(b) == 0);
  }
  {
    Child c;
    assert(do_thing(c) == 1);
    assert(do_other_thing(c) == 1);
  }
  {
    TemplateChild<ParentA> c;
    assert(do_thing(c) == 1);
    assert(do_other_thing(c) == 1);
  }
  return 0;
}
