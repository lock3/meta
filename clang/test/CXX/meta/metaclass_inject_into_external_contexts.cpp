// RUN: %clang_cc1 -freflection -std=c++2a %s

#include "reflection_query.h"

#define assert(E) if (!(E)) __builtin_abort();

using info = decltype(^void);

namespace ns { }

struct simple_struct {
  int var = 0;

  consteval {
    info simple = ^simple_struct;

    -> namespace fragment namespace {
      int get_val(const typename [: %{simple} :] &inst) {
        return inst.var;
      }
    };
  }
};

consteval void metafn(info source) {
  -> namespace(::) fragment namespace {
    struct global_foo {
      int var = 1;

      consteval {
        info global = ^global_foo;
        -> namespace fragment namespace {
          int get_val(const typename [: %{global} :] &inst) {
            return inst.var;
          }
        };
      }
    };
  };

  -> namespace(ns) fragment namespace {
    struct ns_foo {
      int var = 2;

      consteval {
        info ns = ^ns_foo;
        -> namespace fragment namespace {
          int get_val(const typename [: %{ns} :] &inst) {
            return inst.var;
          }
        };
      }
    };
  };

  -> namespace fragment namespace {
    struct parent_ns_foo {
      int var = 3;

      consteval {
        info parent_ns = ^parent_ns_foo;
        -> namespace fragment namespace {
          int get_val(const typename [: %{parent_ns} :] &inst) {
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
