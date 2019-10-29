#include <experimental/meta>
#include <iostream>

using namespace std::experimental;

struct logger {
  logger(std::ostream& os)
    : os(os)
    {}

  logger& operator<<(const char* str) {
    os << str;
    return *this;
  }

  logger& operator<<(int a) {
    os << a;
    return *this;
  }


private:
  std::ostream& os;
};

template<typename T>
T min(T a, T b) {

  constexpr meta::info meta_a = reflexpr(a);
  constexpr meta::info meta_b = reflexpr(b);

  T val_a = valueof(meta_a);
  T val_b = valueof(meta_b);

  logger log(std::cout);

  log << "function: min<"
    << meta::name(reflexpr(T))
    << ">("
    << meta::name(meta_a) << ": "
    << meta::name(meta::type(meta_a)) << "="
    << val_a << ", "
    << meta::name(meta_b) << ": "
    << meta::name(meta::type(meta_b))
    << "=" << val_b << ")\n";

  T result = a < b ? a : b;

  log << meta::name(reflexpr(result)) << ": "
    << meta::name(meta::type(reflexpr(result)))
    << "=" << result;

  log << "\n";

  return result;
}

int main() {
  int a = 33;
  int b = 66;
  min(a, b);
}
