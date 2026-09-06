#ifndef ADB_KEY_PRIV_H
#define ADB_KEY_PRIV_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <adb/adb_error.h>

typedef struct adb_key adb_key_t;
typedef struct adb_ctx adb_ctx_t;

bool adb__key_write_pkcs8_pem(
        adb_key_t *key,
        uint8_t *out,
        const size_t out_size);

bool adb__key_write_x509_pem(
        adb_key_t *key,
        adb_ctx_t *ctx,
        uint8_t *out,
        const size_t out_size);

#endif
