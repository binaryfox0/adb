#ifndef ADB_CONN_H
#define ADB_CONN_H

#include <stddef.h>
#include <adb/adb_error.h>

typedef struct adb_ctx adb_ctx_t;
typedef struct adb_conn adb_conn_t;
typedef struct adb_conn_info adb_conn_info_t;

adb_error_t adb_conn_query(
        adb_ctx_t *ctx,
        adb_conn_info_t ***infos,
        size_t *count);

#endif