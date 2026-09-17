#ifndef ADB_SOCK_H
#define ADB_SOCK_H

#include <stdint.h>
#ifdef _WIN32
#   include <winsock2.h>
#else
#   include <sys/socket.h>
#endif

#include <adb/adb_error.h>

adb_error_t adb__sockaddr_get_host(
        const struct sockaddr *addr,
        char *out_buf,
        const size_t size);

uint16_t adb__sockaddr_get_port(
        const struct sockaddr *addr);

#endif
