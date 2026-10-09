#ifndef ADB_QUERY_PRIV_H
#define ADB_QUERY_PRIV_H

#include <stdbool.h>
#include <stdint.h>

#include "adb_sock.h"
#include "adb_dynarr.h"
#include "adb_compiler.h"

#define ADB__MDNS_NAME_LENGTH_MAX   256

typedef struct libusb_device libusb_device;
typedef struct adb_wired_info
{
    libusb_device *device;

    uint16_t vendor_id;
    uint16_t product_id;
    uint8_t itf_idx;
    uint8_t read_ep;
    uint8_t write_ep;

    char manufacturer[256];
    char product[256];
    char serial[256];
} adb_wired_info_t;

typedef struct adb_wireless_info 
{
    char hostname[ADB__MDNS_NAME_LENGTH_MAX];

    struct sockaddr_in sin;
    struct sockaddr_in6 sin6;

    bool have_srv;
    bool have_ipv4;
    bool have_ipv6;
} adb_wireless_info_t;

adb_error_t adb__query_mdns(
        const char *name,
        const size_t name_len,
        const bool find_mode,
        adb__dynarr_t *out,
        size_t *out_count);

ADB__INLINE bool adb__wireless_info_valid(
        const adb_wireless_info_t *info) {
    return info->have_ipv4 || info->have_ipv6;
}

void adb__wired_info_destroy(
        adb_wired_info_t *info);

void adb__wireless_info_destroy(
        adb_wireless_info_t *info);

#endif
