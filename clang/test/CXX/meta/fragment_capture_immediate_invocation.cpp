// RUN: %clang_cc1 -fsyntax-only -verify -std=c++2a -freflection %s
// expected-no-diagnostics

#include "reflection_query.h"

namespace meta {

consteval info type_of(info reflection) {
  return __reflect(query_get_type, reflection);
}

consteval const char *name_of(meta::info reflection) {
  return __reflect(query_get_name, reflection);
}

}

consteval void gen_member(meta::info member) {
  -> fragment struct {
    void unqualid("set_", meta::name_of(%{member}))(const typename(meta::type_of(%{member}))& unqualid(meta::name_of(%{member}))) {
      this->unqualid(meta::name_of(%{member})) = unqualid(meta::name_of(%{member}));
    }
  };

  -> fragment struct {
    typename(meta::type_of(%{member})) unqualid("get_", meta::name_of(%{member}))() {
      return unqualid(meta::name_of(%{member}));
    }
  };
}

consteval void gen_members(meta::info clazz) {
  gen_member(__reflect(query_get_begin, clazz));
  gen_member(__reflect(query_get_next, __reflect(query_get_begin, clazz)));
}

class book_model {
  const char * author_name;
  int page_count;

public:
  book_model(const char *& author_name, const int& page_count)
    : author_name(author_name), page_count(page_count) { }

  consteval {
    gen_members(reflexpr(book_model));
  }
};

int main() {
  return 0;
}
