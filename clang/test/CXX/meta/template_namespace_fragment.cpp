// RUN: %clang_cc1 -freflection -std=c++2a %s

#define assert(E) if (!(E)) __builtin_abort();

using info = decltype(^void);

template<int val>
consteval auto thing(info ret_ty) {
  return fragment namespace {
    typename [: %{ret_ty} :] build_r() {
      return typename [: %{ret_ty} :]{ val };
    }
  };
}

struct R {
  int val;
};

constexpr auto frag = thing<10>(^R);

namespace r_space {
  consteval -> frag;
}

int main() {
  assert(r_space::build_r().val == 10);
  return 0;
}
