#include <experimental/meta>

using namespace std::experimental;

struct S {
  int a = 0;
  int b = 1;
  int c = 2;
};

template<meta::info X, typename T>
constexpr std::size_t hash_helper(const T& t, std::size_t result) {
  if constexpr (!is_null(X)) {
    if constexpr (is_data_member(X)) {
      auto ptr = valueof(X);

      result ^= t.*ptr;
    }
  }

  if constexpr (next(X) != 0)
    return hash_helper<next(X), T>(t, result);
  else
    return result;
}

template<typename T>
constexpr std::size_t hash(const T& t) {
  std::size_t result = 0u;
  result = hash_helper<meta::front(reflexpr(T)), T>(t, result);

  return result;
}

int main()
{
  S s;
  hash(s);
}
