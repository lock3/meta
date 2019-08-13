// This test checks that the implicit gsl::Owner/Pointer attributes are properly added.

#include <vector>
using namespace std;

auto f() {
    auto i = std::vector<bool>{}.begin();
    std::vector<int> v;
    return v.begin(); // expected-warning {{address of stack memory}}
}

int& f_vector_at() {
    return std::vector<int>{}.at(0); // expected-warning {{returning reference to local temporary object}}
}

#include <string>
auto f_string() {
    return std::string{}.c_str(); // expected-warning {{returning address of local temporary object}}
}

#include <set>
auto f2() {
   return std::set<int>{}.rbegin(); // expected-warning {{returning address of local temporary}}
}

#include <array>
auto f3() {
   return std::array<int,4>{}.rend(); // expected-warning {{returning address of local temporary}}
}

#include <deque>
auto f4() {
   return std::deque<float>{}.rbegin(); // expected-warning {{returning address of local temporary}}
}

#include <forward_list>
auto f5() {
   return std::forward_list<float>{}.begin(); // expected-warning {{returning address of local temporary}}
}

#include <list>
float& f6() {
   return std::list<float>{}.back(); // expected-warning {{returning reference to local temporary object}}
}

#include <map>
#ifndef _LIBCPP_VERSION //TODO
auto f7() {
    return std::map<float, std::string>{}.begin(); // expected-warning {{returning address of local temporary}}
}
#endif

#ifndef _LIBCPP_VERSION //TODO
auto f8() {
   return std::multimap<float, std::string>{}.begin(); // expected-warning {{returning address of local temporary}}
}
#endif

auto f9() {
   return std::multiset<std::string>{}.begin(); // expected-warning {{returning address of local temporary}}
}

#include <queue>
const float& f10() {
   return std::priority_queue<float>{}.top(); // TODOexpected-warning {{returning address of local temporary}}
}

float& f11() {
   return std::queue<float>{}.back(); // expected-warning {{returning reference to local temporary object}}
}

#include <stack>
bool& f12() {
   return std::stack<bool>{}.top(); // TODOexpected-warning {{returning reference to local temporary object}}
}

#include <unordered_map>
auto f13() {
   return std::unordered_map<float, std::string>{}.begin(); // expected-warning {{returning address of local temporary}}
}

auto f14() {
   return std::unordered_multimap<float, std::string>{}.begin(); // expected-warning {{returning address of local temporary}}
}

#include <unordered_set>
auto f15() {
   return std::unordered_multiset<std::string>{}.begin(); // expected-warning {{returning address of local temporary}}
}

auto f16() {
   return std::unordered_set<std::string>{}.begin(); // expected-warning {{returning address of local temporary}}
}

#include <functional>
std::reference_wrapper<int> f_reference_wrapper() {
    int i;
    return {i}; // TODOexpected-warning {{returning address of local temporary}}
}

#include <regex>
auto f_regex_iterator() {
    static const std::string s = "Quick brown fox.";
    static std::regex words_regex("[^\\s]+");
    return std::sregex_iterator(s.begin(), s.end(), words_regex);
}

#if __has_include(<optional>)
#include <optional>
int& f_optional() {
    std::optional<int> o;
    return o.value();
}

int& f_optional2() {
    std::optional<int> o;
    return *o; // expected-warning {{reference to stack memory associated with local variable 'o' returned}}
}
#endif

#include <memory>
int* f_unique_ptr() {
    unique_ptr<int> u{new int};
    return u.get(); // expected-warning {{address of stack memory associated with local variable 'u' returned}}
}

#if __has_include(<any>)
#include <any>
int& f_any() {
    std::any a = 1;
    return std::any_cast<int&>(a); // TODO
}
#endif

#if __has_include(<variant>)
#include <variant>
int& f_variant() {
    std::variant<int, float> v = 12;
    return std::get<int>(v); // TODO
}
#endif

#if __has_include(<string_view>)
#include <string_view>
string_view f_string_view() {
    std::string s;
    return s; // expected-warning {{address of stack memory associated with local variable 's' returned}}
}
#endif
