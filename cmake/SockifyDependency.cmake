# -----------------------------------------------------------------------------
# SockifyDependency.cmake - Generic dependency helper for Sockify
#
# Provides a function to check if a dependency should be used via find_package
# or fetched using FetchContent. It uses a variable named SOCKIFY_USE_<LIBRARY>
# (in all uppercase) to determine this:
# - If SOCKIFY_USE_<LIBRARY> is defined and its value is:
#   - "ON" or Boolean TRUE, then find_package() is used.
#   - A non-empty string, then this string is treated as the path to the
#     dependency and is provided to find_package(PACKAGE PATH ...).
# - Otherwise, FetchContent_Declare() is used to download the dependency.
#
# Usage:
#   sockify_find_or_fetch(<LIBRARY>
#                          [URL <download_url>]
#                          [GIT_REPOSITORY <repo_url>]
#                          [GIT_TAG <git_tag>]
#                          [FIND_COMPONENTS <components>...]
#                          [FIND_PACKAGE_ARGS <args>...])
#
# -----------------------------------------------------------------------------

include(FetchContent)

function(sockify_find_or_fetch LIBRARY)
  set(options)
  set(oneValueArgs URL GIT_REPOSITORY GIT_TAG FIND_PACKAGE_ARGS)
  set(multiValueArgs FIND_COMPONENTS)
  cmake_parse_arguments(SFD "${options}" "${oneValueArgs}" "${multiValueArgs}"
                        ${ARGN})

  string(TOUPPER "${LIBRARY}" LIB_UPPER)
  set(use_var "SOCKIFY_USE_${LIB_UPPER}")

  # Check if SOCKIFY_USE_<LIBRARY> is defined.
  if(DEFINED ${use_var})
    if(${use_var})
      # If the variable is non-empty, assume it might be a path. If not, simply
      # use find_package with provided FIND_PACKAGE_ARGS.
      if(${use_var} STREQUAL "ON")
        message(STATUS "Using system-provided ${LIBRARY} via find_package()")
        find_package(${LIBRARY} ${SFD_FIND_PACKAGE_ARGS} ${SFD_FIND_COMPONENTS})
      else()
        message(STATUS "Using ${LIBRARY} from specified path: ${${use_var}}")
        find_package(${LIBRARY} PATHS ${${use_var}} ${SFD_FIND_PACKAGE_ARGS}
                     ${SFD_FIND_COMPONENTS})
      endif()
    else()
      message(STATUS "${LIBRARY} usage disabled via ${use_var}")
    endif()
  else()
    message(
      STATUS
        "SOCKIFY_USE_${LIB_UPPER} not defined, fetching ${LIBRARY} via FetchContent"
    )
    if(SFD_GIT_REPOSITORY)
      FetchContent_Declare(
        ${LIBRARY}
        GIT_REPOSITORY ${SFD_GIT_REPOSITORY}
        GIT_TAG ${SFD_GIT_TAG})
    elseif(SFD_URL)
      FetchContent_Declare(${LIBRARY} URL ${SFD_URL})
    else()
      message(FATAL_ERROR "No URL or GIT_REPOSITORY provided for ${LIBRARY}")
    endif()
    FetchContent_MakeAvailable(${LIBRARY})
  endif()
endfunction()
