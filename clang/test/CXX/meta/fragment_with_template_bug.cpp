// RUN: %clang -freflection -std=c++2a %s

template<typename T, int TV>
struct struct_s {
  consteval {
    int a = 1;
    -> fragment struct {
      template<typename Q, int QV>
      int unqualid("foo_", %{a})() {
        T t = TV;
        Q q = QV;
        return t + q;
      }
    };
  }
};

int main() {
  struct_s<int, 1> s;
  s.foo_1<int, 2>();
  return 0;
}
