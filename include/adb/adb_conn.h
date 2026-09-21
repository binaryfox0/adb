#ifndef ADB_CONN_H
#define ADB_CONN_H

#include <stdint.h>
#include <stddef.h>
#include <adb/adb_error.h>

typedef struct adb_ctx adb_ctx_t;
typedef struct adb_conn adb_conn_t;
typedef struct adb_wired_info adb_wired_info_t;
typedef struct adb_wireless_info adb_wireless_info_t;
typedef struct adb_key adb_key_t; 

typedef int (*adb_read_fn)(
        void *userdata,
        uint8_t *buf,
        size_t size);

typedef int (*adb_write_fn)(
        void *userdata,
        const uint8_t *buf,
        size_t size);

typedef enum
{
    ADB_CONN_PROFILE_WIRED,
    ADB_CONN_PROFILE_WIRELESS,
    ADB_CONN_PROFILE_CUSTOM,
    ADB__CONN_PROFILE_COUNT
} adb_conn_profile_t;

adb_error_t adb_conn_create_wired_from_info(
        adb_conn_t **conn,
        adb_ctx_t *ctx,
        const adb_wired_info_t *info);

adb_error_t adb_conn_create_wireless(
        adb_conn_t **conn,
        adb_ctx_t *ctx,
        const char *host,
        const uint16_t port);

adb_error_t adb_conn_create_wireless_from_info(
        adb_conn_t **conn,
        adb_ctx_t *ctx,
        const adb_wireless_info_t *info);

adb_error_t adb_conn_create_custom(
        adb_conn_t **conn,
        adb_ctx_t *ctx,
        const adb_read_fn read_cb,
        const adb_write_fn write_cb,
        void *userdata,
        const adb_conn_profile_t profile);


adb_error_t adb_handshake(
        adb_conn_t *conn,
        adb_key_t *key);

adb_error_t adb_pull(
        adb_conn_t *conn,
        const char *path,
        const adb_write_fn write_fn,
        void *userdata);

void adb_conn_destroy(
        adb_conn_t *conn);

#endif
