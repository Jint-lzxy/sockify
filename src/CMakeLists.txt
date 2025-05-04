set(abs_top_srcdir
    "${CMAKE_CURRENT_LIST_DIR}/sockify"
    CACHE INTERNAL "Top-level source directory for Sockify")

function(expand_sources out_var base_dir files)
  foreach(file IN LISTS ${files})
    list(APPEND result "${base_dir}/${file}")
  endforeach()

  set(${out_var}
      "${result}"
      PARENT_SCOPE)
endfunction()

# -----------------------------------------------------------------------------
# Collect source files under lib/ and cli/
# -----------------------------------------------------------------------------

# Leaf filenames only
set(LIBSOCKIFY_SOURCE_FILES error.cpp address.cpp unix_address.cpp
                            ipvx_address.cpp)
set(CLI_SOURCE_FILES main.cpp)

# Expand to full paths
expand_sources(LIBSOCKIFY_SOURCES "${abs_top_srcdir}/lib"
               LIBSOCKIFY_SOURCE_FILES)
expand_sources(CLI_SOURCES "${abs_top_srcdir}/cli" CLI_SOURCE_FILES)

# -----------------------------------------------------------------------------
# Library Target
# -----------------------------------------------------------------------------
set(LIBSOCKIFY_TARGET
    "${PROJECT_NAME}"
    CACHE STRING "Name of the library target")
add_library(${LIBSOCKIFY_TARGET} SHARED ${LIBSOCKIFY_SOURCES})
target_include_directories(${LIBSOCKIFY_TARGET}
                           PUBLIC "${abs_top_srcdir}/include")

# Conditionally define SOCKIFY_BUILD_DLL for Windows
if(WIN32)
  target_compile_definitions(${LIBSOCKIFY_TARGET} PRIVATE SOCKIFY_BUILD_DLL)
endif()

# -----------------------------------------------------------------------------
# CLI Executable Target
# -----------------------------------------------------------------------------
set(SOCKIFY_CLI_TARGET
    "nc"
    CACHE STRING "Name of the CLI target")

if(SOCKIFY_BUILD_CLI)
  add_executable(${SOCKIFY_CLI_TARGET} ${CLI_SOURCES})
  target_link_libraries(${SOCKIFY_CLI_TARGET} PRIVATE ${LIBSOCKIFY_TARGET})
endif()
