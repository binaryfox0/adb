#ifndef ADB_CONN_H
#define ADB_CONN_H

#include <stdint.h>
#include <stddef.h>
#include <adb/adb_error.h>

typedef struct adb_ctx adb_ctx_t;
typedef struct adb_conn adb_conn_t;
typedef struct adb_usb_info adb_usb_info_t;

typedef adb_error_t (*adb_read_callback_t)(
        void *userdata,
        void *buf,
        const size_t size);

typedef adb_error_t (*adb_write_callback_t)(
        void *userdata,
        const void *buf,
        const size_t size);

const char *adb_conn_info_get_manufacturer(
        const adb_usb_info_t *conn_info);
const char *adb_conn_info_get_product(
        const adb_usb_info_t *conn_info);

adb_error_t adb_query_usb(
        adb_ctx_t *ctx,
        adb_usb_info_t ***usb_infos,
        size_t *usb_count);

adb_error_t adb_conn_create_from_info(
        adb_conn_t **conn,
        const adb_usb_info_t *conn_info);

adb_error_t adb_conn_create_wireless(
        adb_conn_t **conn,
        const char *ip,
        const uint16_t port);

adb_error_t adb_conn_create_custom(
        adb_conn_t **conn,
        const adb_read_callback_t read_cb,
        const adb_write_callback_t write_cb,
        void *userdata);

adb_error_t adb_conn_pair(
        adb_conn_t *conn,
        const char code[7]);

void adb_conn_destroy(
        adb_conn_t *conn);

#endif
