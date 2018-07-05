include(CMakePackageConfigHelpers)

get_filename_component(CMAKE_FILES_DIRECTORY_NAME ${CMAKE_FILES_DIRECTORY} NAME)

# Generate package configuration files for the build tree.
set(LIBCPPX_INCLUDE_DIR include)
set(LIBCPPX_EXTRA_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
configure_package_config_file(
  libcppx-config.cmake.in
  libcppx-config.cmake
  INSTALL_PREFIX ${PROJECT_BINARY_DIR}
  INSTALL_DESTINATION cmake
  PATH_VARS LIBCPPX_INCLUDE_DIR LIBCPPX_EXTRA_INCLUDE_DIR
  NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

# Export targets from the build tree.
export(EXPORT libcppx-targets)

# Generate package configuration files for the installation tree.
set(LIBCPPX_INCLUDE_DIR ${LIBCPPX_INSTALL_INCLUDE_DIR})
set(LIBCPPX_EXTRA_INCLUDE_DIR)
configure_package_config_file(
  libcppx-config.cmake.in
  ${CMAKE_FILES_DIRECTORY_NAME}/libcppx-config.cmake
  INSTALL_DESTINATION ${LIBCPPX_INSTALL_PACKAGE_DIR}
  PATH_VARS LIBCPPX_INCLUDE_DIR
  NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

if(LIBCPPX_INSTALL_CMAKE_PACKAGE)
  # Export targets from the installation tree.
  install(EXPORT libcppx-targets DESTINATION ${LIBCPPX_INSTALL_PACKAGE_DIR}
    COMPONENT cppx-cmake)

  # Add install rules for the package configuration file.
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/libcppx-config.cmake
    DESTINATION ${LIBCPPX_INSTALL_PACKAGE_DIR}
    COMPONENT cppx-cmake)

  if(NOT IDE_GENERATOR)
    add_custom_target(install-cppx-cmake
      COMMAND ${CMAKE_COMMAND} -D COMPONENT=cppx-cmake
                               -P ${CMAKE_BINARY_DIR}/cmake_install.cmake)
    add_dependencies(install-cppx install-cppx-cmake)
    add_custom_target(install-libcppx-cmake)
    add_dependencies(install-libcppx-cmake install-cppx-cmake)
  endif()
endif()
