// RUN: %clang_cc1 -std=c++2a -freflection -Wno-deprecated-fragment %s

constexpr auto frag = __fragment {
  requires int i;
  i += 10;
};

int first_level_local() {
  int i = 0;
  {
    consteval -> frag;
  }
  return i;
}

int second_level_local() {
  {
    int i = 0;
    consteval -> frag;
    return i;
  }
}

template<typename T>
T first_level_templ_local() {
  T i = 0;
  {
    consteval -> frag;
  }
  return i;
}

template<typename T>
T second_level_templ_local() {
  {
    T i = 0;
    consteval -> frag;
    return i;
  }
}

int main() {
  int res_1 = first_level_local();
  int res_2 = second_level_local();
  int res_3 = first_level_templ_local<int>();
  int res_4 = second_level_templ_local<int>();
  return 0;
}
