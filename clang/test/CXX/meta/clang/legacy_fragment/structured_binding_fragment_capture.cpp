// RUN: %clang_cc1 -freflection -Wno-deprecated-fragment -std=c++2a %s

// FIXME: How do we test this in the test suite?
// #include <tuple>

// consteval auto tuple_destructure() {
//   return std::make_tuple(true, false);
// }

// class TupleDestructure {
//   consteval {
//     auto [a, b] = tuple_destructure();
//     -> __fragment struct {
//       constexpr auto val_a() {
//         return a;
//       }

//       constexpr auto val_b() {
//         return b;
//       }
//     };
//   }
// };

struct DestructureableClass {
  bool a = true;
  bool b = false;
};

consteval DestructureableClass class_destructure() {
  return DestructureableClass();
}

class ClassDestructure {
  consteval {
    auto [a, b] = class_destructure();
    -> __fragment struct {
      constexpr auto val_a() {
        return a;
      }

      constexpr auto val_b() {
        return b;
      }
    };
  }
};

class ArrayDestructure {
  consteval {
    constexpr bool array_destructure [2] = { true, false };
    auto [a, b] = array_destructure;
    -> __fragment struct {
      constexpr auto val_a() {
        return a;
      }

      constexpr auto val_b() {
        return b;
      }
    };
  }
};

int main() {
  // {
  //   TupleDestructure destructure;
  //   static_assert(destructure.val_a());
  //   static_assert(!destructure.val_b());
  // }
  {
    ClassDestructure destructure;
    static_assert(destructure.val_a());
    static_assert(!destructure.val_b());
  }
  {
    ArrayDestructure destructure;
    static_assert(destructure.val_a());
    static_assert(!destructure.val_b());
  }
}
