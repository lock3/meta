// RUN: %clang_cc1 -freflection -verify -std=c++1z %s

constexpr bool string_eq(const char *s1, const char *s2) {
  while (*s1 != '\0' && *s1 == *s2) {
    s1++;
    s2++;
  }

  return *s1 == *s2;
}

int main() {
  constexpr auto str = __concatenate("foo", "_", "bar");
  static_assert(string_eq(str, "foo_bar"));
  return 0;
}
