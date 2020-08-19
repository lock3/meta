// RUN: %clang -freflection -Wno-deprecated-fragment -std=c++2a %s

template<typename T>
class foo {
  consteval {
    ([](auto cap_ty, auto cap_name) consteval {
      -> __fragment struct {
        typename(cap_ty) unqualid(cap_name)() {
          return 0;
        }
      };
     })(reflexpr(int), reflexpr(foo));
  }
};

int main() {
  foo<float> f;
  return f.foo();
}
