#ifndef ADB_UTILS_H
#define ADB_UTILS_H

#include <stdint.h>
#include <stddef.h>
#include <adb/adb_error.h>

#define ADB__MIN(a, b) ((a) < (b) ? (a) : (b))
#define ADB__IN_RANGE(val, start, end) ((val) >= (start) && (val) <= (end))
#define ADB__CHECK_ENUM(val, pref) ((val) > 0 || (val) < ADB__##pref##_COUNT)
#define ADB__ARRSZ(arr) (sizeof((arr)) / sizeof((arr)[0]))
#define ADB__MEMSZ(s, m) (sizeof(((s*)0)->m))
#define ADB__CONST_CAST(type, var) ((type)(uintptr_t)(var))
#define ADB__ENUM_KEY_VALUE(val) [(val)] = #val 

#define ADB__STRINGIFY_IMPL(x) #x
#define ADB__STRINGIFY(x) ADB__STRINGIFY_IMPL(x)

#define adb__mempcpy(dest, src, n) \
    ((void*)((uint8_t*)memcpy((dest), (src), (n)) + (n)))

static inline uint32_t adb__endian_swap32(
        const uint32_t x)
{
    return 
        ((x & 0x000000FF) << 24) |
        ((x & 0x0000FF00) <<  8) |
        ((x & 0x00FF0000) >>  8) |
        ((x & 0xFF000000) >> 24);
}

adb_error_t adb__util_read_file(
        const char *path,
        char **buf_out,
        size_t *size_out);

adb_error_t adb__util_write_file(
        const char *path,
        const uint8_t *buf,
        const size_t size);

#endif
