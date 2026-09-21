#ifndef ADB_PAIR_H
#define ADB_PAIR_H

#include <stdint.h>
#include <adb/adb_error.h>

typedef struct adb_ctx adb_ctx_t;
typedef struct adb_conn adb_conn_t;
typedef struct adb_key adb_key_t; 
typedef struct adb_wireless_info adb_wireless_info_t;

adb_error_t adb_pair(
        adb_conn_t *conn,
        const char *code,
        adb_key_t *key,
        char *out_guid,
        const size_t out_guid_len);

adb_error_t adb_pair_qr_build_payload(
        adb_ctx_t *ctx,
        char *out_service,
        const size_t service_size,
        char *out_secret,
        const size_t secret_size);

adb_error_t adb_pair_qr_encode_payload(
        const char *service_name,
        const char *secret,
        char *out,
        const size_t size);

adb_error_t adb_pair_qr(
        adb_conn_t *conn,
        const char *secret,
        adb_key_t *key,
        char *out_guid,
        const size_t out_guid_len);


#endif
