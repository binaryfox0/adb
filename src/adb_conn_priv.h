#ifndef ADB_CONN_PRIV_H
#define ADB_CONN_PRIV_H

#include <stdbool.h>
#include <stddef.h>

#include <adb/adb_error.h>
#include <adb/adb_conn.h>
#include "adb_sock.h"

typedef struct adb_ctx adb_ctx_t;
typedef struct adb_conn adb_conn_t;
typedef struct adb__tls adb__tls_t;

adb_error_t adb__conn_from_sockaddr(
        adb_conn_t **conn,
        adb_ctx_t *ctx,
        const struct sockaddr *addr);

adb_error_t adb__conn_upgrade_tls(
        adb_conn_t *conn,
        adb_key_t *key);

adb_error_t adb__conn_read(
        adb_conn_t *conn,
        void *buf,
        const size_t size);

adb_error_t adb__conn_read_alloc(
        adb_conn_t *conn,
        void **out,
        const size_t size);

adb_error_t adb__conn_write(
        adb_conn_t *conn,
        const void *buf,
        const size_t size);

uint32_t adb__conn_get_max_payload_size(
        adb_conn_t *conn);

adb__tls_t *adb__conn_get_tls(
        adb_conn_t *conn);

adb_ctx_t *adb__conn_get_ctx(
        adb_conn_t *conn);


#endif
