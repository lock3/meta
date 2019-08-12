#include <vector>
auto f() {
    std::vector<int> v;
    return v.begin(); // expected-warning {{address of stack memory}}
}


#include <set>
auto f2() {
   return std::set<int>{}.begin(); // expected-warning {{returning address of local temporary}}
}

#if 0
static_assert(__is_gsl_owner(std::vector<int>), "");
static_assert(__is_gsl_pointer(std::vector<int>::iterator), "");
static_assert(__is_gsl_pointer(std::vector<int>::const_iterator), "");
static_assert(__is_gsl_pointer(std::vector<int>::reverse_iterator), "");
static_assert(__is_gsl_pointer(std::vector<int>::const_reverse_iterator), "");
#endif

#if 0
#include <string>
static_assert(__is_gsl_owner(std::string), "");
static_assert(__is_gsl_pointer(std::string::iterator), "");
static_assert(__is_gsl_pointer(std::string::const_iterator), "");
static_assert(__is_gsl_pointer(std::string::reverse_iterator), "");
static_assert(__is_gsl_pointer(std::string::const_reverse_iterator), "");

#if __has_include(<optional>)
#include <optional>
static_assert(__is_gsl_owner(std::optional<int>), "");
#endif

#if __has_include(<any>)
#include <any>
static_assert(__is_gsl_owner(std::any), "");
#endif

#if __has_include(<variant>)
#include <variant>
static_assert(__is_gsl_owner(std::variant<int, char>), "");
#endif

#if __has_include(<string_view>)
#include <string_view>
static_assert(__is_gsl_pointer(std::string_view), "");
static_assert(__is_gsl_pointer(std::string_view::iterator), "");
static_assert(__is_gsl_pointer(std::string_view::const_iterator), "");

static_assert(__is_gsl_pointer(std::wstring_view), "");
static_assert(__is_gsl_pointer(std::wstring_view::iterator), "");
static_assert(__is_gsl_pointer(std::wstring_view::const_iterator), "");
#endif
#endif
