cmake_minimum_required(VERSION 3.16.0)

# Enable folder grouping for IDEs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Detect whether Sockify is being bundled into another
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  set(SOCKIFY_BUNDLE_LIBRARY OFF)
else()
  set(SOCKIFY_BUNDLE_LIBRARY ON)
endif()

project(
  Sockify
  VERSION 0.1.0 # CML version placeholder, don't delete
  LANGUAGES CXX
  HOMEPAGE_URL "https://github.com/Jint-lzxy/sockify"
  DESCRIPTION "A modern, C++-native, socket library.")

# -----------------------------------------------------------------------------
# Options
# -----------------------------------------------------------------------------

option(DEVELOPMENT_BUILD
       "Enable development build settings (build tests, enable -Werror, etc.)"
       OFF)

# Use cmake_dependent_option to configure dependent options. The tests will be
# built if DEVELOPMENT_BUILD is enabled and if we're not bundling.
include(CMakeDependentOption)
cmake_dependent_option(SOCKIFY_BUILD_TESTS "Build tests for Sockify" ON
                       "DEVELOPMENT_BUILD;NOT SOCKIFY_BUNDLE_LIBRARY" OFF)
cmake_dependent_option(
  SOCKIFY_ENABLE_COVERAGE "Enable code coverage instrumentation" ON
  "DEVELOPMENT_BUILD" OFF)
cmake_dependent_option(
  SOCKIFY_ENABLE_WERROR "Enable -Werror to treat warnings as errors" ON
  "DEVELOPMENT_BUILD" OFF)

# -----------------------------------------------------------------------------
# Set Compiler Flags
# -----------------------------------------------------------------------------

# Enable almost all warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -pedantic)
  if(SOCKIFY_ENABLE_WERROR)
    add_compile_options(-Werror)
  endif()
elseif(MSVC)
  add_compile_options(/W4)
  if(SOCKIFY_ENABLE_WERROR)
    add_compile_options(/WX)
  endif()
endif()

# -----------------------------------------------------------------------------
# Subdirectories
# -----------------------------------------------------------------------------

# Add the 'src' directory for the library (and any optional command line tools)
add_subdirectory(src)

# Only build tests if SOCKIFY_BUILD_TESTS is enabled
if(SOCKIFY_BUILD_TESTS)
  add_subdirectory(tests)
endif()
