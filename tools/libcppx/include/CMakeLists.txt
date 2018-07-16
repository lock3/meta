set(LIBCPPX_HEADERS cppx/compiler cppx/meta cppx/traits.hpp cppx/tuple.hpp)

# Group all headers together in generated IDE projects (otherwise, header files
# without extensions may not be placed in the "Header Files" group).
source_group("Header Files" FILES ${LIBCPPX_HEADERS})

# Set properties on source files.
set_source_files_properties(cppx/compiler cppx/meta PROPERTIES
  XCODE_EXPLICIT_FILE_TYPE sourcecode.cpp.h)
set_source_files_properties(${LIBCPPX_HEADERS} PROPERTIES HEADER_FILE_ONLY
  TRUE)

set(LIBCPPX_HEADER_PATTERN
  PATTERN "compiler"
  PATTERN "meta"
  PATTERN "*.hpp")

# Copy the headers to the build tree if we're building Clang.
if(LIBCPPX_BUILDING_CLANG)
  # Install these headers in the build version of the resource dir
  set(output_dir ${LLVM_LIBRARY_OUTPUT_INTDIR}/clang/${CLANG_VERSION}/include)

  message(STATUS "Copying CPPX headers")
  file(COPY cppx DESTINATION ${output_dir}
    FILES_MATCHING ${LIBCPPX_HEADER_PATTERN})

  # Force reconfiguration when headers are modified so that the updated headers
  # are copied to the build tree.
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${LIBCPPX_HEADERS})
endif()

if(LIBCPPX_INSTALL_HEADERS)
  # Add install rules for the library's header files.
  install(DIRECTORY cppx
    DESTINATION ${LIBCPPX_INSTALL_INCLUDE_DIR}
    COMPONENT cppx-headers
    FILES_MATCHING ${LIBCPPX_HEADER_PATTERN})

  if(NOT IDE_GENERATOR)
    add_custom_target(install-cppx-headers
      COMMAND ${CMAKE_COMMAND} -D COMPONENT=cppx-headers
                               -P ${CMAKE_BINARY_DIR}/cmake_install.cmake)
    add_custom_target(install-libcppx-headers)
    add_dependencies(install-libcppx-headers install-cppx-headers)
  endif()
endif()

# Add a dummy target for IDE generators so that the library's header files are
# added to the generated project file.
if(IDE_GENERATOR)
  add_custom_target(libcppx SOURCES ${LIBCPPX_HEADERS})
endif()

if(NOT LIBCPPX_BUILDING_CLANG)
  # Add 'cppx' library target. This library is header-only.
  add_library(cppx INTERFACE)

  if(CXX_COMPILER_HAS_STDCXX1Z_FLAG)
    target_compile_options(cppx INTERFACE -std=c++1z)
  endif()
  if(CXX_COMPILER_HAS_FREFLECTION_FLAG)
    target_compile_options(cppx INTERFACE -Xclang -freflection)
  endif()

  target_include_directories(cppx INTERFACE
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include;${PROJECT_BINARY_DIR}/include>"
    $<INSTALL_INTERFACE:${LIBCPPX_INSTALL_INCLUDE_DIR}>)

  # Add install rules for the 'cppx' library target.
  install(TARGETS cppx EXPORT libcppx-targets COMPONENT cppx)
endif()

if(NOT IDE_GENERATOR)
  if(NOT LIBCPPX_BUILDING_CLANG)
    add_custom_target(install-cppx
      COMMAND ${CMAKE_COMMAND} -D COMPONENT=cppx
                               -P ${CMAKE_BINARY_DIR}/cmake_install.cmake)
    add_dependencies(install-cppx cppx)
  else()
    add_custom_target(install-cppx)
  endif()
  if(LIBCPPX_INSTALL_HEADERS)
    add_dependencies(install-cppx install-cppx-headers)
  endif()
  add_custom_target(install-libcppx)
  add_dependencies(install-libcppx install-cppx)
endif()
