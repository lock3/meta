// RUN: %clang_cc1 -freflection -std=c++2a %s

namespace meta {
  using info = decltype(reflexpr(void));
}

class noncopyable {
  noncopyable(const noncopyable&) = delete;
};

consteval void foo_meta_fn(meta::info clazz) {
}

class(foo_meta_fn) foo_meta_class {
  noncopyable f;
};

int main() {
  return 0;
}
