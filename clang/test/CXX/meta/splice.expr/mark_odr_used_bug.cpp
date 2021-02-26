// RUN: %clang -freflection -std=c++2a %s

template<typename Lambda>
void call_lambda(Lambda &&lambda) {
  [: ^lambda :]("hi");
}

int main() {
  call_lambda([](const char *str) { });
  return 0;
}
