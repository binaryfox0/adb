#ifndef ADB_TRANSPORT_TCP_H
#define ADB_TRANSPORT_TCP_H

#include <stdint.h>
#include <adb/adb_error.h>

typedef struct adb_wired_info adb_wired_info_t;
typedef struct adb__transport adb__transport_t;

adb_error_t adb__tcp_transport_create(
        adb__transport_t *transport,
        const char *host,
        const uint16_t port);

#endif
