// RUN: %clang_cc1 -freflection -std=c++1z %s

constexpr bool string_eq(const char *s1, const char *s2) {
  while (*s1 != '\0' && *s1 == *s2) {
    s1++;
    s2++;
  }

  return *s1 == *s2;
}

constexpr const char * join_strings(const char *s1, const char *s2) {
  return __concatenate(true ? s1 : "false", " ", s2);
}

int main() {
  constexpr auto str = join_strings("foo", "bar");
  static_assert(string_eq(str, "foo bar"));
  return 0;
}
