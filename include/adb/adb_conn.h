#ifndef ADB_CONN_H
#define ADB_CONN_H

#include <stdint.h>
#include <stddef.h>
#include <adb/adb_error.h>

typedef struct adb_ctx adb_ctx_t;
typedef struct adb_conn adb_conn_t;

typedef size_t (*adb_read_callback_t)(
        void *userdata,
        void *buf,
        const size_t size);

typedef size_t (*adb_write_callback_t)(
        void *userdata,
        const void *buf,
        const size_t size);

typedef enum
{
    ADB_CONN_TYPE_WIRED,
    ADB_CONN_TYPE_WIRELESS,
    ADB_CONN_TYPE_CUSTOM,
    ADB__CONN_TYPE_COUNT
} adb_conn_type_t;

adb_error_t adb_conn_create_wired(
        adb_conn_t **conn,
        adb_ctx_t *ctx,
        const adb_usb_info_t *usb_info);

adb_error_t adb_conn_create_wireless(
        adb_conn_t **conn,
        adb_ctx_t *ctx,
        const char *ip,
        const uint16_t port);

adb_error_t adb_conn_create_custom(
        adb_conn_t **conn,
        adb_ctx_t *ctx,
        const adb_read_callback_t read_cb,
        const adb_write_callback_t write_cb,
        void *userdata);

adb_error_t adb_conn_pair(
        adb_conn_t *conn,
        const char code[7]);

adb_error_t adb_conn_handshake(
        adb_conn_t *conn);

void adb_conn_destroy(
        adb_conn_t *conn);

#endif
