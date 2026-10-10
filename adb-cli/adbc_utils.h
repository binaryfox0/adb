#ifndef ADBC_UTILS_H
#define ADBC_UTILS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ADBC_ARRSZ(arr) (sizeof((arr)) / sizeof((arr)[0]))

const char *adbc_util_home_dir(void);
bool adbc_util_parse_endpoint(
        const char *input,
        char *out_host,
        const size_t size,
        uint16_t *out_port);
bool adbc_util_file_exist(
        const char *path);
uint64_t adbc_util_monotonic_ms(void);

#endif
