#ifndef ADB_CONN_PRIV_H
#define ADB_CONN_PRIV_H

#include <stdbool.h>
#include <stddef.h>
#include <adb/adb_error.h>
#include <adb/adb_conn.h>

typedef struct adb_ctx adb_ctx_t;
typedef struct adb_conn adb_conn_t;
typedef struct adb__tls adb__tls_t;

adb__tls_t *adb__conn_get_tls(
        adb_conn_t *conn);

adb_error_t adb__conn_read(
        adb_conn_t *conn,
        void *buf,
        const size_t size);

adb_error_t adb__conn_write(
        adb_conn_t *conn,
        const void *buf,
        const size_t size);

#endif
