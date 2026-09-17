#ifndef ADB_QUERY_PRIV_H
#define ADB_QUERY_PRIV_H

#include <stdint.h>
#include "adb_sock.h"

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
    struct sockaddr addr;
} adb_wireless_info_t;

void adb__wired_info_destroy(
        adb_wired_info_t *info);

void adb__wireless_info_destroy(
        adb_wireless_info_t *info);

#endif
