// RUN: %clang -freflection -std=c++2a %s

#include "reflection_query.h"

consteval const char *name_of(meta::info reflection) {
  return __reflect(query_get_name, reflection);
}

template<typename Lambda>
void call_lambda(Lambda &&lambda) {
  consteval -> fragment {
    [: %{^lambda} :](name_of(%{^int}));
  };
}

int main() {
  call_lambda([](const char *name) { });
  return 0;
}
