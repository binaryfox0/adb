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
            -fsanitize=address,leak,undefined)
    else()
        message(FATAL_ERROR "AddressSanitizer was not supported by "
            "'${CMAKE_C_COMPILER_ID}'")
    endif()
endfunction()

find_package(MbedTLS REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(libusb libusb-1.0 REQUIRED IMPORTED_TARGET)
pkg_check_modules(lz4 liblz4 REQUIRED IMPORTED_TARGET)
pkg_check_modules(zstd libzstd REQUIRED IMPORTED_TARGET)
pkg_check_modules(brotlidec libbrotlidec REQUIRED IMPORTED_TARGET)

find_package(PkgConfig REQUIRED)
pkg_check_modules(lz4 REQUIRED IMPORTED_TARGET liblz4)

include(CheckCSourceCompiles)

set(CMAKE_REQUIRED_LIBRARIES PkgConfig::lz4)
set(CMAKE_REQUIRED_DEFINITIONS -DLZ4F_STATIC_LINKING_ONLY)

check_c_source_compiles("
#include <stddef.h>
#include <lz4frame.h>

int main(void)
{
    (void)LZ4F_createDecompressionContext_advanced(
        (LZ4F_CustomMem){0},
        LZ4F_VERSION);
    return 0;
}
" HAVE_LZ4F_CREATE_DECOMPRESSION_CONTEXT_ADVANCED)
if(NOT HAVE_LZ4F_CREATE_DECOMPRESSION_CONTEXT_ADVANCED)
    include(FetchContent)

    set(LZ4_BUILD_CLI OFF CACHE BOOL "" FORCE)
    set(LZ4_BUILD_LEGACY_LZ4C OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
    set(LZ4_BUNDLED_MODE ON CACHE BOOL "" FORCE)

    FetchContent_Declare(
        lz4
        GIT_REPOSITORY https://github.com/lz4/lz4.git
        GIT_TAG v1.10.0
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR build/cmake/
    )

    FetchContent_MakeAvailable(lz4)
else()
    add_library(lz4 ALIAS PkgConfig::lz4)
endif()

add_subdirectory(deps/mdns)
add_subdirectory(spake2)

add_library(adb
    src/adb_alloc.c
    src/adb_error.c
    src/adb_log.c
    src/adb_ctx.c
    src/adb_lookup.c
    src/adb_query.c
    src/adb_transport_usb.c
    src/adb_transport_tcp.c
    src/adb_transport_custom.c
    src/adb_transport.c
    src/adb_tls.c
    src/adb_utils.c
    src/adb_key.c
    src/adb_aes.c
    src/adb_packet.c
    src/adb_sock.c
    src/adb_pair.c
    src/adb_decomp.c
    src/adb_sync.c
    src/adb_conn.c)
target_compile_definitions(adb PRIVATE
    ZSTD_STATIC_LINKING_ONLY
    LZ4F_STATIC_LINKING_ONLY)
target_include_directories(adb PUBLIC include/)
target_link_libraries(adb PRIVATE 
    PkgConfig::libusb
    MbedTLS::mbedtls
    MbedTLS::mbedx509
    MbedTLS::mbedcrypto
    spake2
    mdns
    lz4
    PkgConfig::zstd
    PkgConfig::brotlidec)
target_add_strict_flags(adb)
target_add_asan(adb)

if(ADB_BUILD_CLI)
    add_subdirectory(deps/qrcodegen)
    if(NOT TARGET aparse)
        add_subdirectory(deps/aparse)
    endif()

    add_executable(adb-cli adb-cli/main.c)
    target_link_libraries(adb-cli PRIVATE aparse adb qrcodegen)
    target_add_strict_flags(adb-cli)
    target_add_asan(adb-cli)
endif()
