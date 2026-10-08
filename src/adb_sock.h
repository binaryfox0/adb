#ifndef ADB_SOCK_H
#define ADB_SOCK_H

#include <stdint.h>
#ifdef _WIN32
#   include <winsock2.h>
#   include <ws2tcpip.h>
#else
#   include <sys/socket.h>
#   include <arpa/inet.h>
#endif

#include <adb/adb_error.h>

adb_error_t adb__sockaddr_host(
        const struct sockaddr *addr,
        char *out_buf,
        const size_t size);

uint16_t adb__sockaddr_port(
        const struct sockaddr *addr);

adb_error_t adb__sockaddr_endpoint(
        const struct sockaddr *addr,
        char *out_buf,
        const size_t buf_size);

const char *adb__sockaddr_endpoint_local(
        const struct sockaddr *addr);

adb_error_t adb__sock_timeout(
        const int sock,
        const uint64_t deadline_ms);

#endif
