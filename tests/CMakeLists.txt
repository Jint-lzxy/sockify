# We require at least Catch2 v3.0.1
include(SockifyDependency)
sockify_find_or_fetch(
  Catch2
  GIT_REPOSITORY "https://github.com/catchorg/Catch2.git"
  GIT_TAG "v3.8.0"
  FIND_PACKAGE_ARGS 3.0.1 REQUIRED
)

# When using FetchContent (i.e. if Catch2 wasn't found via find_package), add
# its extras directory to CMAKE_MODULE_PATH so that Catch2's integration scripts
# are available
if(NOT TARGET Catch2::Catch2)
  list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
endif()

include(CTest)
include(Catch)

# -----------------------------------------------------------------------------
# Create Test Executable
# -----------------------------------------------------------------------------
set(SOCKIFY_TEST_TARGET "${PROJECT_NAME}_tests")
set(SOCKIFY_TEST_SOURCES test_main.cpp)

add_executable(${SOCKIFY_TEST_TARGET} ${SOCKIFY_TEST_SOURCES})
target_link_libraries(${SOCKIFY_TEST_TARGET} PRIVATE ${LIBSOCKIFY_TARGET}
                                                     Catch2::Catch2WithMain)

catch_discover_tests(${SOCKIFY_TEST_TARGET} ADD_TAGS_AS_LABELS)
