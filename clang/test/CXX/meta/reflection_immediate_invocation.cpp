// RUN: %clang -std=c++2a -freflection %s

using info = decltype(^void);

consteval void do_something_with_info(info x) {
}

int main() {
  do_something_with_info(^void);
  return 0;
}
