#ifndef ADB_UTILS_H
#define ADB_UTILS_H

#include <stdint.h>
#include <stddef.h>
#include <adb/adb_error.h>

#define ADB__MIN(a, b) ((a) < (b) ? (a) : (b))
#define ADB__IN_RANGE(val, start, end) ((val) >= (start) && (val) < (end))

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
