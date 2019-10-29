#include <experimental/meta>
#include <cstdio>

namespace meta = std::experimental::meta;

enum Enum {
  A,
  B,
  C,
  D
};

template<meta::info enumerator, typename E, E e>
consteval const char* to_string_helper() {
  constexpr auto meta_enum = valueof(enumerator);

  if constexpr(meta_enum == e)
    return meta::name(enumerator);

  if constexpr(meta::next(enumerator) != 0)
    return to_string_helper<meta::next(enumerator), E, e>();

  return nullptr;
}

template<meta::info enumeration, typename E, E e>
consteval const char* to_string() {
  constexpr meta::info first = meta::front(enumeration);

  if constexpr(first == 0)
    return nullptr;

  return to_string_helper<first, E, e>();
}

int main()
{
  const char* n = to_string<reflexpr(Enum), Enum, A>();

  printf("to_string(A) = %s\n", to_string<reflexpr(Enum), Enum, A>());
  printf("to_string(B) = %s\n", to_string<reflexpr(Enum), Enum, B>());
  printf("to_string(C) = %s\n", to_string<reflexpr(Enum), Enum, C>());
  printf("to_string(D) = %s\n", to_string<reflexpr(Enum), Enum, D>());
}
