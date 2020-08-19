// RUN: %clang_cc1 -freflection -std=c++2a %s

using string_type = const char *;

constexpr bool string_eq(string_type s1, string_type s2) {
  while (*s1 != '\0' && *s1 == *s2) {
    s1++;
    s2++;
  }

  return *s1 == *s2;
}

constexpr string_type join_strings(string_type s1, string_type s2) {
  return __concatenate(true ? s1 : "false", " ", s2);
}

int main() {
  constexpr auto str = join_strings("foo", "bar");
  static_assert(string_eq(str, "foo bar"));
  return 0;
}
