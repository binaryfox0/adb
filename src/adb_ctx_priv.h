#ifndef ADB_CTX_PRIV_H
#define ADB_CTX_PRIV_H

#include <stddef.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include "adb_dynarr.h"

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
    adb__dynarr_t wired_infos;

    adb__dynarr_t wireless_infos;
    size_t wireless_info_count;
    adb__dynarr_t pair_winfos;
    size_t pair_winfo_count;
    adb__dynarr_t dev_winfos;
    size_t dev_winfo_count;

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context drbg;
} adb_ctx_t;


#endif
