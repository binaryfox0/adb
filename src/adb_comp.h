#ifndef ADB_COMP_H
#define ADB_COMP_H

#include <stdbool.h>
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
        const adb__comp_type_t type);

ADB__NODISCARD adb_error_t adb__comp_compress(
        adb__comp_t *comp,
        const void *data,
        const size_t size,
        void *output,
        const size_t output_size,
        size_t *input_used,
        size_t *output_used);

ADB__NODISCARD adb_error_t adb__comp_finish(
        adb__comp_t *comp,
        void *output,
        const size_t output_size,
        size_t *output_used);

ADB__NODISCARD bool adb__comp_is_done(
        adb__comp_t *comp);

void adb__comp_destroy(
        adb__comp_t *comp);

#endif
