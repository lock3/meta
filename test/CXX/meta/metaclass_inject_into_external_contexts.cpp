// RUN: %clang_cc1 -freflection -std=c++1z %s

#include "reflection_query.h"

using info = decltype(reflexpr(void));

namespace ns { }

consteval void metafn(info source) {
  -> namespace(::) __fragment namespace {
    class global_foo {
      int var = 1;
    };
  };

  -> namespace(ns) __fragment namespace {
    class ns_foo {
      int var = 2;
    };
  };

  -> namespace __fragment namespace {
    class parent_ns_foo {
        int var = 3;
    };
  };
}

namespace meta_class_ns {
  class(metafn) metaclass {
  };
}

consteval info definition_of(info type_reflection) {
  return __reflect(query_get_definition, type_reflection);
}

consteval void compiler_print_type_definition(info type_reflection) {
  (void) __reflect_pretty_print(definition_of(type_reflection));
}

consteval {
  compiler_print_type_definition(reflexpr(::global_foo));
  compiler_print_type_definition(reflexpr(ns::ns_foo));
  compiler_print_type_definition(reflexpr(meta_class_ns::parent_ns_foo));
}

int main() {
  return 0;
}

