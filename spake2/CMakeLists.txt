cmake_minimum_required(VERSION 3.16)
project(spake2)

option(SPAKE2_BUILD_TESTS "Build spake2 tests" OFF)

find_package(MbedTLS REQUIRED)
find_package(OpenSSL REQUIRED)

add_library(spake2
    src/spake2_fe.c
    src/spake2_ge.c
    src/spake2_sc.c
    src/spake2_u256.c
    src/spake2.c)
target_include_directories(spake2 PUBLIC include/)
target_link_libraries(spake2 PRIVATE MbedTLS::mbedcrypto)
#target_add_strict_flags(spake2)

if(SPAKE2_BUILD_TESTS)
    if(NOT TARGET aparse)
        add_subdirectory(deps/aparse)
    endif()

    add_executable(test tests/main.c)
    target_link_libraries(test PRIVATE spake2)
endif()
