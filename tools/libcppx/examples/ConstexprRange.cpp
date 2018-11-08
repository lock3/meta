#include <cstdio>
#include <iterator>

template <typename T, unsigned N>
struct array {
  const T data[N];

  template<typename... Args>
  constexpr array(const Args&... args) : data{args...} {
}
  struct array_iterator {
    T const* ptr;

    constexpr array_iterator(const T* ptr) : ptr(ptr) {}
    constexpr array_iterator(const array<T, N>& arr) : ptr(arr.data) {}

    constexpr void operator++() { ++ptr; }
    constexpr void operator--() { --ptr; }
    constexpr T const& operator* () const { return *ptr; }
    constexpr bool operator==(const array_iterator& rhs) const { return ptr == rhs.ptr; }
    constexpr bool operator!=(const array_iterator& rhs) const { return !(*this == rhs); }
    constexpr int operator-(const array_iterator& rhs) const { return ptr - rhs.ptr; }
  };

  constexpr array_iterator begin() const { return array_iterator(data); }
  constexpr array_iterator end()   const { return array_iterator(data + N); }
};

template<typename forward_it>
consteval forward_it next(forward_it it, int n = 1) { return forward_it(it.ptr + n); }

template<typename input_it>
consteval int distance(input_it begin, input_it end) {
  return end - begin;
}

int main()
{
  static constexpr array<int, 3> v = {0, 1, 2};
  static constexpr int arr[3] = {0, 1, 2};

  for constexpr (auto x : arr) {printf("%d\t", x);}
  printf("\n");
  for constexpr (auto x : v) {printf("%d\t", x);}
  printf("\n");
}
