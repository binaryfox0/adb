#ifndef ADB_CTX_H
#define ADB_CTX_H

#include <adb/adb_error.h>

typedef struct adb_ctx adb_ctx_t;

adb_error_t adb_ctx_create(
        adb_ctx_t **ctx);
void adb_ctx_destroy(
        adb_ctx_t *ctx);

#endif