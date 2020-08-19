// RUN: %clang_cc1 -freflection -Wno-deprecated-fragment -std=c++2a %s

#define assert(E) if (!(E)) __builtin_abort();

template<int val>
consteval auto thing() {
  return __fragment namespace {
    requires typename R;

    R build_r() {
      return R { val };
    }
  };
}

constexpr auto frag = thing<10>();

struct R {
  int val;
};

namespace r_space {
  consteval -> frag;
}

int main() {
  assert(r_space::build_r().val == 10);
  return 0;
}
