#ifndef ADB_TRANSPORT_TCP_H
#define ADB_TRANSPORT_TCP_H

#include <stdint.h>
#include <stdbool.h>

#include <adb/adb_error.h>
#include "adb_sock.h"

typedef struct adb_wired_info adb_wired_info_t;
typedef struct adb__transport adb__transport_t;

bool adb__tcp_sockaddr_from_host_port(
        struct sockaddr *out,
        const char *host,
        const uint16_t port);

adb_error_t adb__tcp_transport_create(
        adb__transport_t *transport,
        const struct sockaddr *addr);

#endif
