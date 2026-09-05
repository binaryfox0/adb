#ifndef ADB_CTX_PRIV_H
#define ADB_CTX_PRIV_H

#include <stddef.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>

typedef struct libusb_context libusb_context;
typedef struct adb_wired_info adb_wired_info_t;

enum
{
    ADB__FEATURE_WIRED    = (1 << 0),
    ADB__FEATURE_WIRELESS = (1 << 1)
};

typedef struct adb_ctx
{
    uint32_t features;

    libusb_context *usb;
    adb_wired_info_t **wired_infos;
    size_t wired_info_count;
    size_t infos_capacity;

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context drbg;
} adb_ctx_t;


#endif
