cmake_minimum_required(VERSION 3.16)
project(spake2)

option(SPAKE2_BUILD_TESTS "Build spake2 tests" OFF)

find_package(MbedTLS REQUIRED)
find_package(OpenSSL REQUIRED)

add_library(spake2
    src/spake2.c)
target_include_directories(spake2 PUBLIC include/)
target_link_libraries(spake2 PRIVATE MbedTLS::mbedcrypto)

if(SPAKE2_BUILD_TESTS)
    if(NOT TARGET aparse)
        add_subdirectory(deps/aparse)
    endif()
endif()
