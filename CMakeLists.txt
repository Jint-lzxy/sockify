cmake_minimum_required(VERSION 3.16.0)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project(
    Sockify
    VERSION 0.1.0 # CML version placeholder, don't delete
    LANGUAGES CXX
    HOMEPAGE_URL "https://github.com/Jint-lzxy/sockify"
    DESCRIPTION "A modern, C++-native, socket library.")

add_subdirectory(src)
add_subdirectory(tests)
