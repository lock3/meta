// RUN: %clang_cc1 -emit-llvm -std=c++2a -freflection -o %t %s

using info = decltype(^void);

constexpr struct is_const_type_fn
{
  consteval bool operator()(info type) const {
    return false;
  }
} is_const_type;

int main() {
  return is_const_type(^int);
}
