cmake_minimum_required(VERSION 3.15)
project(adb)

option(ADB_BUILD_WITH_ASAN "Build adb with AddressSanitizer" OFF)
option(ADB_BUILD_CLI "Build the adb cli tool" OFF)

function(target_add_strict_flags target)
    if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
            $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:
                -Wall
                -Wextra
                -Wpedantic
                -Werror
                -Wconversion
                -Wsign-conversion
                -Wdouble-promotion
                -Wshadow
                -Wundef
                -Wcast-align
                -Wcast-qual
                -Wwrite-strings
                -Wstrict-prototypes
                -Wmissing-prototypes
                -Wold-style-definition
                -Wswitch-enum
                -Wswitch-default
                -Wimplicit-fallthrough
                -Wformat
                -Wformat-overflow
                -Wformat-truncation
                -Wformat-signedness
                -Warray-bounds
                -Wnull-dereference
                -Wvla
                
                -Wno-variadic-macro-arguments-omitted
                -Wno-gnu-zero-variadic-macro-arguments

                -fdiagnostics-show-option
                -fno-common
            >
        )
    endif()
endfunction()

function(target_add_asan target)
    if(NOT ADB_BUILD_WITH_ASAN)
        return()
    endif()

    if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        if(NOT (CMAKE_BUILD_TYPE MATCHES "[Dd]ebug"
            OR CMAKE_BUILD_TYPE MATCHES "RelWithDebInfo"))
            message(FATAL_ERROR "ADB_BUILD_WITH_ASAN MUST "
                "be built with Debug or RelWithDebInfo")
        endif()
        target_compile_options(${target} PRIVATE
            -fsanitize=address,leak,undefined
            -fno-omit-frame-pointer)

        target_link_options(${target} PRIVATE
            -fsanitize=address)
    else()
        message(FATAL_ERROR "AddressSanitizer was not supported by "
            "'${CMAKE_C_COMPILER_ID}'")
    endif()
endfunction()

find_package(PkgConfig REQUIRED)
pkg_check_modules(libusb libusb-1.0 REQUIRED IMPORTED_TARGET)

add_library(adb
    src/adb_alloc.c
    src/adb_error.c
    src/adb_log.c
    src/adb_ctx.c)
target_include_directories(adb PUBLIC include/)
target_link_libraries(adb PRIVATE PkgConfig::libusb)
target_add_strict_flags(adb)
target_add_asan(adb)

if(ADB_BUILD_CLI)
    add_subdirectory(deps/aparse)
    add_executable(adb-cli adb-cli/main.c)
    target_link_libraries(adb-cli PRIVATE aparse adb)
    target_add_strict_flags(adb-cli)
    target_add_asan(adb-cli)
endif()