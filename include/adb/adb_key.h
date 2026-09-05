#ifndef ADB_KEY_H
#define ADB_KEY_H

#include <adb/adb_error.h>

typedef struct adb_key adb_key_t;
typedef struct adb_ctx adb_ctx_t;

adb_error_t adb_key_generate(
        adb_key_t **key,
        adb_ctx_t *ctx);

adb_error_t adb_key_load(
        adb_key_t **key,
        adb_ctx_t *ctx,
        const char *path);

adb_error_t adb_key_save(
        adb_key_t *key,
        const char *path);

adb_error_t adb_key_generate_pubkey(
        adb_key_t *key,
        char **out);

void adb_key_destroy(
        adb_key_t *key);

#endif
