#ifndef ADB_CONN_PRIV_H
#define ADB_CONN_PRIV_H

#include <stdbool.h>
#include <stddef.h>

#include <adb/adb_error.h>
#include <adb/adb_conn.h>
#include "adb_sock.h"

#define ADB__FEATURE_SENDRECV_V2        "sendrecv_v2"
#define ADB__FEATURE_SENDRECV_V2_BROTLI "sendrecv_v2_brotli"
#define ADB__FEATURE_SENDRECV_V2_LZ4    "sendrecv_v2_lz4"
#define ADB__FEATURE_SENDRECV_V2_ZSTD   "sendrecv_v2_zstd"
#define ADB__FEATURE_DELAYED_ACK        "delayed_ack"

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

bool adb__has_feature(
        adb_conn_t *conn,
        const char *feature);

bool adb__feature_support(
        const char *feature);


#endif
