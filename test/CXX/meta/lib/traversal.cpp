// // RUN: %clang_cc1 -freflection -std=c++2a %s

// namespace ns {
//   template<typename T, typename U, typename R>
//   void fn(T a, U b, R c) {
//   }
// }

// using info = decltype(reflexpr(void));

// constexpr info ns_rf = reflexpr(ns);
// constexpr info test_rf = *range(ns_rf).begin();
// constexpr info template_parm1 = __reflect(query_get_begin_template_param, test_rf);
// constexpr info template_parm2 = __reflect(query_get_next_template_param, template_parm1);
// constexpr info template_parm3 = __reflect(query_get_next_template_param, template_parm2);
// constexpr info template_parm4 = __reflect(query_get_next_template_param, template_parm3); // expected-error {{constexpr variable 'template_parm4' must be initialized by a constant expression}} // expected-note {{query is not defined for reflected construct}}

