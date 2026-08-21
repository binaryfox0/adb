#ifndef ADB_CONN_H
#define ADB_CONN_H

#include <stddef.h>
#include <adb/adb_error.h>

typedef struct adb_ctx adb_ctx_t;
typedef struct adb_conn adb_conn_t;
typedef struct adb_conn_info adb_conn_info_t;

const char *adb_conn_info_get_manufacturer(
        const adb_conn_info_t *conn_info);
const char *adb_conn_info_get_product(
        const adb_conn_info_t *conn_info);

adb_error_t adb_query_conn(
        adb_ctx_t *ctx,
        adb_conn_info_t ***conn_infos,
        size_t *conn_count);

adb_error_t adb_conn_create(
        adb_conn_t **conn,
        const adb_conn_info_t *conn_info);

void adb_conn_destroy(
        adb_conn_t *conn);

#endif