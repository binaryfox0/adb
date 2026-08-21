#ifndef ADB_CTX_PRIV_H
#define ADB_CTX_PRIV_H

#include <stddef.h>

typedef struct libusb_context libusb_context;
typedef struct adb_conn_info adb_conn_info_t;
typedef struct adb_ctx
{
    libusb_context *usb;

    adb_conn_info_t **conn_infos;
    size_t infos_count;
    size_t infos_capacity;
} adb_ctx_t;


#endif