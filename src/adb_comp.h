#ifndef ADB_COMP_H
#define ADB_COMP_H

#include <adb/adb_error.h>
#include <adb/adb_conn.h>
#include "adb_compiler.h"

typedef enum
{
    ADB__COMP_NONE = 0,
    ADB__COMP_LZ4,
    ADB__COMP_ZSTD,
    ADB__COMP_BROTLI,
    ADB__COMP_COUNT
} adb__comp_type_t;

typedef struct adb__comp adb__comp_t;

ADB__NODISCARD adb_error_t adb__comp_create(
        adb__comp_t **comp,
        const adb__comp_type_t type,
        const adb_write_fn write_fn,
        void *userdata);

ADB__NODISCARD size_t adb__comp_input_bound(
        adb__comp_t *comp,
        const size_t out_max);

ADB__NODISCARD adb_error_t adb__comp_compress(
        adb__comp_t *comp,
        const void *data,
        const size_t size);

ADB__NODISCARD adb_error_t adb__comp_finish(
        adb__comp_t *comp);

void adb__comp_destroy(
        adb__comp_t *comp);

#endif
