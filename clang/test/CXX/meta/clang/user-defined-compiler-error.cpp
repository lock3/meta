// RUN: %clang_cc1 -freflection -verify -std=c++2a %s

consteval void report_error() {
  __compiler_error("Error reported!"); // expected-error {{Error reported!}}
}

consteval void report_error(const char *custom_message) {
  __compiler_error(custom_message); // expected-error {{Custom error reported!}}
}

int main() {
  report_error(); // expected-error {{call to consteval function 'report_error' is not a constant expression}} expected-note {{in call to 'report_error()'}}
  report_error("Custom error reported!"); // expected-error {{call to consteval function 'report_error' is not a constant expression}} expected-note {{in call to 'report_error(&"Custom error reported!"[0])'}}
  return 0;
}
