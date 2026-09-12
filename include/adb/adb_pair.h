#ifndef ADB_PAIR_H
#define ADB_PAIR_H

#include <stdint.h>
#include <adb/adb_error.h>

typedef struct adb_ctx adb_ctx_t;
typedef struct adb_key adb_key_t; 

adb_error_t adb_pair(
        adb_ctx_t *ctx,
        const char *host,
        const uint16_t port,
        const char *code,
        const size_t code_len,
        adb_key_t *key);

#endif
