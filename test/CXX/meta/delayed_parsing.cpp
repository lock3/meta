// RUN: %clang_cc1 -freflection -fdelayed-template-parsing -std=c++1z %s

namespace meta {
  using info = decltype(reflexpr(void));
}

consteval void virtual_destructor(meta::info source) {
  -> __fragment struct X {
    virtual ~X() noexcept { }
  };
};


class(virtual_destructor) Shape {
};

class Square : public Shape {
};

int main() {
  Square s;
  return 0;
}
