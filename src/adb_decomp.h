#ifndef ADB__DECOMP_H
#define ADB__DECOMP_H

#include <adb/adb_error.h>
#include <adb/adb_conn.h>
#include "adb_compiler.h"

typedef struct adb__decomp adb__decomp_t;
typedef enum
{
    ADB__DECOMP_NONE,
    ADB__DECOMP_ZSTD,
    ADB__DECOMP_LZ4,
    ADB__DECOMP_BROTLI,
    ADB__DECOMP_COUNT
} adb__decomp_type_t;

ADB__NODISCARD adb_error_t adb__decomp_create(
        adb__decomp_t **decomp,
        const adb__decomp_type_t type,
        const adb_write_fn write_fn,
        void *userdata);

ADB__NODISCARD adb_error_t adb__decomp_decompress(
        adb__decomp_t *decomp,
        const void *data,
        const size_t size);

void adb__decomp_destroy(
        adb__decomp_t *decomp);

#endif
