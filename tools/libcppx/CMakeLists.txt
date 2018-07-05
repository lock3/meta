# IMPORTANT: Since libcppx requires C++ language features that are exclusive to
# Clang, libcppx *cannot* be built as a part of Clang. For this reason, by
# default, no binary targets for this project will be added if this directory
# is a subdirectory in another project's source tree and a non-imported target
# named 'clang' exists. (The library's header files can still be installed.)

cmake_minimum_required(VERSION 3.5)

# Set CMake module path.
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# FIXME: We don't have a reflection flag just yet.
# include(CheckCXXReflection)

project(libcppx CXX)

if(MSVC_IDE OR GHS-MULTI OR XCODE)
  set(IDE_GENERATOR 1)
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  # libcppx is the top-level source directory, and is being built as a
  # standalone project.
  set(LIBCPPX_STANDALONE_BUILD 1)
else()
  # libcppx is a subdirectory in another project's source tree, and is being
  # built as part of another project.
  # Determine whether libcppx is being built as a part of Clang.
  if(TARGET clang)
    get_target_property(_clang_imported clang IMPORTED)
    if(NOT _clang_imported)
      message(STATUS "libcppx is building as part of Clang")
      set(LIBCPPX_BUILDING_CLANG 1)
    endif()
  endif()
endif()

# Determine default values for configuration settings.
set(LIBCPPX_LIBDIR_SUFFIX_DEFAULT "")
set(_libdir_suffix_found FALSE)
if(NOT LIBCPPX_STANDALONE_BUILD)
  # Sync LIBCPPX_LIBDIR_SUFFIX with whatever value the LLVM build configuration
  # used (or, if not defined, whatever the Clang build configuration used).
  foreach(prefix LLVM CLANG)
    if(DEFINED ${prefix}_LIBDIR_SUFFIX)
      set(LIBCPPX_LIBDIR_SUFFIX_DEFAULT "${${prefix}_LIBDIR_SUFFIX}")
      set(_libdir_suffix_found TRUE)
      break()
    endif()
  endforeach()
endif()
# Otherwise, if CMAKE_INSTALL_LIBDIR is defined and begins with "lib", use the
# remainder of the string as the default value for LIBCPPX_LIBDIR_SUFFIX.
if(NOT _libdir_suffix_found AND
   (DEFINED CMAKE_INSTALL_LIBDIR AND CMAKE_INSTALL_LIBDIR MATCHES "^lib(.*)$"))
  set(LIBCPPX_LIBDIR_SUFFIX_DEFAULT ${CMAKE_MATCH_1})
  set(_libdir_suffix_found TRUE)
endif()

# Add configuration settings.
set(LIBCPPX_LIBDIR_SUFFIX "${LIBCPPX_LIBDIR_SUFFIX_DEFAULT}" CACHE STRING
    "Define suffix of library directory name (32/64).")
option(LIBCPPX_INCLUDE_EXAMPLES
  "Generate build targets for the libcppx example programs." ON)
option(LIBCPPX_BUILD_EXAMPLES "Build the libcppx example programs." OFF)
option(LIBCPPX_INSTALL_HEADERS "Install the libcppx headers." ON)
option(LIBCPPX_INSTALL_CMAKE_PACKAGE
  "Install the CMake config-file package for libcppx." ON)

set(LIBCPPX_INSTALL_INCLUDE_DIR include)
if(WIN32 AND NOT CYGWIN)
  set(LIBCPPX_INSTALL_PACKAGE_DIR cmake)
else()
  set(LIBCPPX_INSTALL_PACKAGE_DIR lib${LIBCPPX_LIBDIR_SUFFIX}/cmake/libcppx)
endif()

# Do not allow users to set the value of LIBCPPX_LIBDIR_SUFFIX if a suitable
# libdir suffix was found.
if(_libdir_suffix_found)
  unset(LIBCPPX_LIBDIR_SUFFIX CACHE)
  set(LIBCPPX_LIBDIR_SUFFIX "${LIBCPPX_LIBDIR_SUFFIX_DEFAULT}")
endif()

# If libcppx is being built as part of Clang, disable options that add binary
# targets.
if(LIBCPPX_BUILDING_CLANG)
  foreach(option INCLUDE_EXAMPLES BUILD_EXAMPLES INSTALL_CMAKE_PACKAGE)
    if(LIBCPPX_${option})
      set_property(CACHE LIBCPPX_${option} PROPERTY VALUE OFF)
      message(STATUS "Disabled option LIBCPPX_${option}")
    endif()
  endforeach()
endif()

# Run platform checks.
if(NOT LIBCPPX_BUILDING_CLANG)
  check_cxx_reflection(CXX_COMPILER_HAS_REFLECTION
    ${PROJECT_SOURCE_DIR}/include ${PROJECT_BINARY_DIR}/include)
  if(NOT CXX_COMPILER_HAS_REFLECTION)
    message(FATAL_ERROR
      "${PROJECT_NAME} requires a C++ compiler that supports reflection.")
  endif()
endif()

# Add subdirectories.
add_subdirectory(include)
if(NOT LIBCPPX_BUILDING_CLANG)
  add_subdirectory(cmake)
endif()
if(NOT LIBCPPX_BUILDING_CLANG AND LIBCPPX_INCLUDE_EXAMPLES)
  add_subdirectory(examples)
endif()
