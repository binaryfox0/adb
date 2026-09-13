#ifndef ADB_MDNS_H
#define ADB_MDNS_H

#include <stdint.h>
#ifdef _WIN32
#   include <winsock2.h>
#else
#   include <sys/socket.h>
#endif
#include <adb/adb_error.h>

adb_error_t adb__mdns_find_service(
        const char *guid,
        struct sockaddr *out);

#endif
