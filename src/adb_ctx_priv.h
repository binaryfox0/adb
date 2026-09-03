#ifndef ADB_CTX_PRIV_H
#define ADB_CTX_PRIV_H

#include <stddef.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>

typedef struct libusb_context libusb_context;
typedef struct adb_wired_info adb_wired_info_t;
typedef struct adb_ctx
{
    libusb_context *usb;
    adb_wired_info_t **conn_infos;
    size_t infos_count;
    size_t infos_capacity;

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context drbg;
} adb_ctx_t;


#endif
