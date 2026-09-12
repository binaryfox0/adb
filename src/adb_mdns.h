#ifndef ADB_MDNS_H
#define ADB_MDNS_H

#include <stdint.h>
#include <adb/adb_error.h>

typedef struct
{
    char *host;
    uint16_t port;
} adb__mdns_info_t;

adb_error_t adb__mdns_find_service(
        const char *guid,
        adb__mdns_info_t *out);

#endif
