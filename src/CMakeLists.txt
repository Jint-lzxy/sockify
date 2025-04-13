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
set(LIBSOCKIFY_SOURCE_FILES entry.cpp)
set(NC_SOURCE_FILES main.cpp)

# Expand to full paths
expand_sources(LIBSOCKIFY_SOURCES "${abs_top_srcdir}/lib"
               LIBSOCKIFY_SOURCE_FILES)
expand_sources(NC_SOURCES "${abs_top_srcdir}/cli" NC_SOURCE_FILES)

# -----------------------------------------------------------------------------
# Library Target
# -----------------------------------------------------------------------------
set(LIBSOCKIFY_TARGET "${PROJECT_NAME}")
add_library(${LIBSOCKIFY_TARGET} SHARED ${LIBSOCKIFY_SOURCES})
target_include_directories(${LIBSOCKIFY_TARGET}
                           PUBLIC "${abs_top_srcdir}/include")

# -----------------------------------------------------------------------------
# CLI Executable Target
# -----------------------------------------------------------------------------
add_executable(nc ${NC_SOURCES})
target_link_libraries(nc PRIVATE ${LIBSOCKIFY_TARGET})
