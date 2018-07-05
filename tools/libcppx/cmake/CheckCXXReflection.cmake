#.rst:
# CheckCXXReflection
# ------------------
#
# Provides a macro for checking whether the C++ compiler supports reflection.
#
# .. command:: check_cxx_reflection
#
#    Check if the C++ compiler supports reflection::
#
#      check_cxx_reflection(<variable> [<include-dir>...])
#
#    ``<variable>``
#      The variable that will store the result.
#    ``<include-dir>...``
#      Additional directories to search for libcppx headers (e.g.
#      ``cppx/meta``).
#
#    The following variables may be set before calling this macro to modify the
#    way the check is run:
#
#    ``CMAKE_REQUIRED_FLAGS``
#      String of compile command-line flags.
#    ``CMAKE_REQUIRED_DEFINITIONS``
#      List of macros to define (``-DFOO=bar``).
#    ``CMAKE_REQUIRED_INCLUDES``
#      List of include directories.
#    ``CMAKE_REQUIRED_LIBRARIES``
#      List of libraries to link.
#    ``CMAKE_REQUIRED_QUIET``
#      Execute quietly without messages.
#
# .. note::
#    As of 2017, the only known compiler that supports this language feature
#    is a private branch of `Clang <http://clang.llvm.org>`_.

include(CMakePushCheckState)
include(CheckCXXCompilerFlag)
include(CheckCXXSourceCompiles)

macro(CHECK_CXX_REFLECTION VARIABLE)
  if(NOT DEFINED "${VARIABLE}" OR "x${${VARIABLE}}" STREQUAL "x${VARIABLE}")
    cmake_push_check_state()

    # Check whether the C++ compiler supports the -std=c++1z and -freflection
    # flags. If so, append the flags to CMAKE_REQUIRED_FLAGS.
    check_cxx_compiler_flag(-std=c++1z CXX_COMPILER_HAS_STDCXX1Z_FLAG)
    if(CXX_COMPILER_HAS_STDCXX1Z_FLAG)
      set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -std=c++1z")
    endif()
    check_cxx_compiler_flag("-Xclang -freflection" CXX_COMPILER_HAS_FREFLECTION_FLAG)
    if(CXX_COMPILER_HAS_FREFLECTION_FLAG)
      set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -Xclang -freflection")
    endif()

    if(${ARGC} GREATER 1)
      # Append extra include directories to CMAKE_REQUIRED_INCLUDES.
      set(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES} ${ARGN})
    endif()

    check_cxx_source_compiles([=[
#include <cppx/meta>

struct foo {};

int main() {
  const bool x = $foo.is_empty();
  return !x;
}
]=] ${VARIABLE})

    cmake_pop_check_state()
  endif()
endmacro()
