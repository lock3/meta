// RUN: %clang_cc1 -freflection -std=c++2a %s

#define assert(E) if (!(E)) __builtin_abort();

namespace meta {
  using info = decltype(^void);
}

struct Member {
  const char* name;
  meta::info type;
};

constexpr Member members[]{
  Member{"a", ^int},
  Member{"b", ^int}
};

consteval void generate_struct(const char* name) {
  -> fragment namespace {
    struct [# %{name} #] {
      consteval {
        for (auto member : %{members}) {
          -> fragment struct {
            typename [: %{member.type} :] [# %{member.name} #];
          };
        }
      }
    };
  };
}

consteval {
  generate_struct("S");
}

int main() {
  S s{1, 2};
  assert(s.a == 1);
  assert(s.b == 2);
  return 0;
}