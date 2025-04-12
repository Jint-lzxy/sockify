# -----------------------------------------------------------------------------
# FindLibEvent.cmake - Dependency helper for locating the libevent library
#
# This script locates the libevent library and its components.
# It exports:
#   LibEvent_FOUND           - Indicates successful location of libevent
#   LIBEVENT_INCLUDE_DIRS    - Path to libevent include directory
#   LIBEVENT_LIBRARIES       - List of libraries required for linking
#
# Additionally, per-component variables are set:
#   LIBEVENT_<COMPONENT>_FOUND    - True if <COMPONENT> found (e.g., CORE)
#   LIBEVENT_<COMPONENT>_LIBRARY  - Path to <COMPONENT> library
#
# Imported targets are created for ease of use:
#   LibEvent::LibEvent       - Unified target
#   LibEvent::Core           - Core component target
#   LibEvent::Extra          - Extra component target
#   LibEvent::Pthread        - Pthread component target (optional)
#   LibEvent::OpenSSL        - OpenSSL component target (optional)
#
# Usage:
#   find_package(LibEvent [<version>] [REQUIRED] [COMPONENTS <components>...])
#
# -----------------------------------------------------------------------------

cmake_policy(PUSH)
cmake_policy(SET CMP0159 NEW) # file(STRINGS) with REGEX updates CMAKE_MATCH_<n>

# Default options if not set externally
if(NOT DEFINED EVHTP_DISABLE_EVTHR)
  set(EVHTP_DISABLE_EVTHR OFF)
endif()
if(NOT DEFINED EVHTP_DISABLE_SSL)
  set(EVHTP_DISABLE_SSL OFF)
endif()

function(libevent_find_component COMPONENT FIND_NAME)
  find_library(
    LIBEVENT_${COMPONENT}
    NAMES ${FIND_NAME}
    HINTS ${PC_LIBEVENT_LIBRARY_DIRS}
    PATHS /usr/local/lib /usr/lib
    DOC "libevent ${COMPONENT} library")

  if(LIBEVENT_${COMPONENT})
    set(LIBEVENT_${COMPONENT}_FOUND
        TRUE
        PARENT_SCOPE)
    set(LIBEVENT_${COMPONENT}_LIBRARY
        ${LIBEVENT_${COMPONENT}}
        PARENT_SCOPE)
  else()
    set(LIBEVENT_${COMPONENT}_FOUND
        FALSE
        PARENT_SCOPE)
    set(LIBEVENT_${COMPONENT}_LIBRARY
        ""
        PARENT_SCOPE)
  endif()
endfunction()

function(libevent_create_imported_target target var)
  if(${var} AND NOT TARGET ${target})
    add_library(${target} UNKNOWN IMPORTED)
    set_target_properties(
      ${target}
      PROPERTIES IMPORTED_LOCATION ${${var}}
                 INTERFACE_INCLUDE_DIRECTORIES "${LIBEVENT_INCLUDE_DIRS}")
  endif()
endfunction()

# Use pkg-config hints if available
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_LIBEVENT QUIET libevent)
endif()

# Locate the libevent include directory
find_path(
  LIBEVENT_INCLUDE_DIR
  NAMES event.h
  HINTS ${PC_LIBEVENT_INCLUDE_DIRS}
  PATHS /usr/local/include /usr/include
  DOC "Path to libevent include directory")

# Locate the unified libevent library (combined library)
find_library(
  LIBEVENT_LIBRARY
  NAMES event
  HINTS ${PC_LIBEVENT_LIBRARY_DIRS}
  PATHS /usr/local/lib /usr/lib
  DOC "Combined libevent library")

# Search for individual components using the helper function
libevent_find_component(CORE event_core)
libevent_find_component(EXTRA event_extra)
if(NOT EVHTP_DISABLE_EVTHR)
  libevent_find_component(PTHREAD event_pthreads)
endif()
if(NOT EVHTP_DISABLE_SSL)
  libevent_find_component(OPENSSL event_openssl)
endif()

# Mark internal search results as advanced
mark_as_advanced(
  LIBEVENT_INCLUDE_DIR
  LIBEVENT_LIBRARY
  LIBEVENT_CORE
  LIBEVENT_EXTRA
  LIBEVENT_PTHREAD
  LIBEVENT_OPENSSL
)

# Aggregate the discovered libraries
set(LIBEVENT_INCLUDE_DIRS ${LIBEVENT_INCLUDE_DIR})
set(LIBEVENT_LIBRARIES ${LIBEVENT_LIBRARY})
if(LIBEVENT_CORE_FOUND)
  list(APPEND LIBEVENT_LIBRARIES ${LIBEVENT_CORE_LIBRARY})
endif()
if(LIBEVENT_EXTRA_FOUND)
  list(APPEND LIBEVENT_LIBRARIES ${LIBEVENT_EXTRA_LIBRARY})
endif()
if(NOT EVHTP_DISABLE_EVTHR AND LIBEVENT_PTHREAD_FOUND)
  list(APPEND LIBEVENT_LIBRARIES ${LIBEVENT_PTHREAD_LIBRARY})
endif()
if(NOT EVHTP_DISABLE_SSL AND LIBEVENT_OPENSSL_FOUND)
  list(APPEND LIBEVENT_LIBRARIES ${LIBEVENT_OPENSSL_LIBRARY})
endif()

# Optionally extract version information from event-config.h
if(LIBEVENT_INCLUDE_DIRS)
  file(
    STRINGS "${LIBEVENT_INCLUDE_DIR}/event2/event-config.h" _ver_line
    REGEX "#define[ \t]+EVENT__VERSION[ \t]+\"[0-9]+\\.[0-9]+\\.[0-9]+.*\""
    LIMIT_COUNT 1)
  if(_ver_line)
    string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" LIBEVENT_VERSION
                 "${_ver_line}")
  endif()
  unset(_ver_line)
endif()

# Validate package configuration
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  LibEvent
  REQUIRED_VARS LIBEVENT_INCLUDE_DIRS LIBEVENT_LIBRARIES
  VERSION_VAR LIBEVENT_VERSION)

# Optionally create a unified imported target for libevent
if(LibEvent_FOUND AND NOT TARGET LibEvent::LibEvent)
  add_library(LibEvent::LibEvent UNKNOWN IMPORTED)
  set_target_properties(
    LibEvent::LibEvent
    PROPERTIES IMPORTED_LOCATION "${LIBEVENT_LIBRARY}"
               INTERFACE_INCLUDE_DIRECTORIES "${LIBEVENT_INCLUDE_DIRS}")
endif()

# Create per-component imported targets
libevent_create_imported_target(LibEvent::Core LIBEVENT_CORE_LIBRARY)
libevent_create_imported_target(LibEvent::Extra LIBEVENT_EXTRA_LIBRARY)
if(NOT EVHTP_DISABLE_EVTHR)
  libevent_create_imported_target(LibEvent::Pthread LIBEVENT_PTHREAD_LIBRARY)
endif()
if(NOT EVHTP_DISABLE_SSL)
  libevent_create_imported_target(LibEvent::OpenSSL LIBEVENT_OPENSSL_LIBRARY)
endif()

cmake_policy(POP)
