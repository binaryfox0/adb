#ifndef ADB_TRANSPORT_USB_H
#define ADB_TRANSPORT_USB_H

#include <adb/adb_error.h>

typedef struct adb_wired_info adb_wired_info_t;
typedef struct adb__transport adb__transport_t;

adb_error_t adb__usb_transport_create(
        adb__transport_t *transport,
        const adb_wired_info_t *info);

#endif

