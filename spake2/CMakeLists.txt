cmake_minimum_required(VERSION 3.16)
project(spake2)

option(SPAKE2_BUILD_TESTS "Build spake2 tests" OFF)

find_package(MbedTLS REQUIRED)

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
    include(FetchContent)

    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(OPENSSL_SMALL ON CACHE BOOL "" FORCE)
    FetchContent_Declare(
        boringssl
        URL
            "https://boringssl.googlesource.com/boringssl/+archive/825a42ab64264250e13bc4b94155d10ed9d92298.tar.gz"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )

    FetchContent_MakeAvailable(boringssl)

    if(NOT TARGET aparse)
        add_subdirectory(deps/aparse)
    endif()

    add_executable(test 
        tests/bssl_spake2.c
        tests/my_spake2.c
        tests/main.c)
    target_link_libraries(test PRIVATE aparse spake2 crypto)
endif()
