#ifndef ADBC_UTILS_H
#define ADBC_UTILS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
#endif

#define ADBC_IP_LENGTH_MAX INET6_ADDRSTRLEN
#define ADBC_ARRSZ(arr) (sizeof((arr)) / sizeof((arr)[0]))

const char *adbc_util_home_dir(void);
bool adbc_util_parse_endpoint(
        const char *input,
        char *out_host,
        const size_t size,
        uint16_t *out_port);
bool adbc_util_file_exist(
        const char *path);
uint64_t adbc_util_monotonic_ms(void);

#endif
