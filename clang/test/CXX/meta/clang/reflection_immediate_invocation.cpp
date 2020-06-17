// RUN: %clang -std=c++2a -freflection %s

using info = decltype(reflexpr(void));

consteval void do_something_with_info(info x) {
}

int main() {
  do_something_with_info(reflexpr(void));
  return 0;
}
