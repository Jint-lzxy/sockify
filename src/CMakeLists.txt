set(abs_top_srcdir
    "${CMAKE_CURRENT_LIST_DIR}/sockify"
    CACHE INTERNAL "Top-level source directory for Sockify")

# -----------------------------------------------------------------------------
# Collect source files under lib/ and cli/
# -----------------------------------------------------------------------------

file(GLOB LIBSOCKIFY_SOURCES "${abs_top_srcdir}/lib/*.cpp")
file(GLOB NC_SOURCES "${abs_top_srcdir}/cli/*.cpp")

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
