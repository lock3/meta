# libcppx

The `cppx` library provides essential library support for compile time
reflection and code generation. This is a header-only library, providing
the following headers:

- cppx/meta — library support for language reflection


## Configuration and building

This is currently a header-only library. It just needs to be copied into
a location where it can be found by the compiler.

You don't even need to run `cmake` or `make`. Just copy and use (see below).


## Using `cppx`

In order to use `cppx`, you will need a C++ compiler that supports the
`-freflection` flag—currently that means a fork of Clang.

Simply add `libcppx/include` to your include search path. Header files should
be included as:

```c++
#include <cppx/meta>
```

Note that that "lib" is not used in the include path for the header files.

With both Clang and `cppx`, you can use reflection to query declarations:
variables, functions, values, types, templates, and namespaces. Currently,
only the reflection of variables, functions, and values is supported.

**TODO:** Document how reflection can be used.

```c++
int main() {
  constexpr auto fn = $main;
  std::cout << fn.name() << '\n';              // prints main
  std::cout << fn.parameters().size() << '\n'; // prints 0
  ...
}
```

