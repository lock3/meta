// RUN: %clang_cc1 -freflection -verify -std=c++2a %s

consteval void report_error() {
  __compiler_error("Error reported!");
}

consteval void report_error(const char *custom_message) {
  __compiler_error(custom_message);
}

consteval{
  -> fragment namespace{
    consteval{ // expected-error {{Error reported!}}
      report_error();
      report_error("Custom error reported!");
    }

    consteval { // expected-error {{Custom error reported!}}
      report_error("Custom error reported!");
    }
  };
}
