cmake_minimum_required(VERSION 3.16.0)

# Enable folder grouping for IDEs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Make sure our custom CMake modules are available
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

project(
  sockify
  VERSION 0.1.0 # CML version placeholder, don't delete
  LANGUAGES CXX
  HOMEPAGE_URL "https://github.com/Jint-lzxy/sockify"
  DESCRIPTION "A modern, C++-native, socket library.")

set(CMAKE_CXX_STANDARD
    17
    CACHE STRING "C++ standard to conform to")
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

# Detect whether Sockify is being bundled into another project
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  set(SOCKIFY_BUNDLE_LIBRARY OFF)
else()
  set(SOCKIFY_BUNDLE_LIBRARY ON)
endif()

# -----------------------------------------------------------------------------
# Options
# -----------------------------------------------------------------------------

# General development settings
option(
  SOCKIFY_DEVELOPMENT_BUILD
  "Enable development build settings (build tests, docs, enable -Werror, etc.)"
  OFF)

# Dependent options for building parts of the project
include(CMakeDependentOption)
cmake_dependent_option(SOCKIFY_BUILD_DOCS "Build documentation" ON
                       "SOCKIFY_DEVELOPMENT_BUILD" OFF)
cmake_dependent_option(
  SOCKIFY_BUILD_TESTS "Build tests for Sockify" ON
  "SOCKIFY_DEVELOPMENT_BUILD;NOT SOCKIFY_BUNDLE_LIBRARY" OFF)
cmake_dependent_option(
  SOCKIFY_ENABLE_COVERAGE "Enable code coverage instrumentation" ON
  "SOCKIFY_DEVELOPMENT_BUILD;NOT SOCKIFY_BUNDLE_LIBRARY" OFF)
cmake_dependent_option(
  SOCKIFY_ENABLE_WERROR "Enable -Werror to treat warnings as errors" ON
  "SOCKIFY_DEVELOPMENT_BUILD" OFF)

# Enforce that Catch2 must be available if tests are being built
if(SOCKIFY_BUILD_TESTS)
  if(DEFINED SOCKIFY_USE_CATCH2 AND NOT SOCKIFY_USE_CATCH2)
    message(
      FATAL_ERROR
        "Catch2 is disabled (SOCKIFY_USE_CATCH2 is OFF) but tests are enabled! Please enable Catch2 or disable tests."
    )
  endif()
endif()

# -----------------------------------------------------------------------------
# Set Compiler Flags
# -----------------------------------------------------------------------------

# Enable a robust level of warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
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
# Code Coverage Setup
# -----------------------------------------------------------------------------
if(SOCKIFY_ENABLE_COVERAGE)
  message(STATUS "Code coverage instrumentation enabled")

  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    # For Clang and GNU, disable optimization to get accurate coverage data,
    # include debugging symbols, and enable coverage instrumentation.
    add_compile_options(-O0 -g --coverage)
    add_link_options(--coverage)
  elseif(MSVC)
    # For MSVC, enable full debug information. Coverage tools on Windows (such
    # as Visual Studio Code Coverage or third-party tools) require full PDB
    # information.
    add_compile_options(/Zi)
    add_link_options(/DEBUG:FULL /INCREMENTAL:NO /PROFILE)
  else()
    message(
      FATAL_ERROR
        "Code coverage is not supported with this compiler (${CMAKE_CXX_COMPILER_ID})"
    )
  endif()
endif()

# -----------------------------------------------------------------------------
# Subdirectories
# -----------------------------------------------------------------------------

add_subdirectory(src)

if(SOCKIFY_BUILD_DOCS)
  add_subdirectory(docs)
endif()

if(SOCKIFY_BUILD_TESTS)
  add_subdirectory(tests)
endif()
