// RUN: %clang_cc1 -freflection -Wno-deprecated-fragment -std=c++2a %s

#include "../reflection_query.h"

#define assert(E) if (!(E)) __builtin_abort();

using info = decltype(reflexpr(void));

namespace ns { }

struct simple_struct {
  int var = 0;

  consteval {
    -> namespace __fragment namespace {
      requires typename simple_struct;

      int get_val(const simple_struct &inst) {
        return inst.var;
      }
    };
  }
};

consteval void metafn(info source) {
  -> namespace(::) __fragment namespace {
    struct global_foo {
      int var = 1;

      consteval {
        -> namespace __fragment namespace {
          requires typename global_foo;

          int get_val(const global_foo &inst) {
            return inst.var;
          }
        };
      }
    };
  };

  -> namespace(ns) __fragment namespace {
    struct ns_foo {
      int var = 2;

      consteval {
        -> namespace __fragment namespace {
          requires typename ns_foo;

          int get_val(const ns_foo &inst) {
            return inst.var;
          }
        };
      }
    };
  };

  -> namespace __fragment namespace {
    struct parent_ns_foo {
      int var = 3;

      consteval {
        -> namespace __fragment namespace {
          requires typename parent_ns_foo;

          int get_val(const parent_ns_foo &inst) {
            return inst.var;
          }
        };
      }
    };
  };
}

namespace parent_ns {
  class(metafn) metaclass {
  };
}

int main() {
  assert(get_val(simple_struct()) == 0);
  assert(get_val(global_foo()) == 1);
  assert(ns::get_val(ns::ns_foo()) == 2);
  assert(parent_ns::get_val(parent_ns::parent_ns_foo()) == 3);

  return 0;
}
