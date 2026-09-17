#include "adb_sock.h"

#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

adb_error_t adb__sockaddr_get_host(
        const struct sockaddr *addr,
        char *out_buf,
        const size_t size)
{
    struct sockaddr_in sin4 = {0};
    struct sockaddr_in6 sin6 = {0};
    if(!addr)
        return ADB_ERR_PARAM;

    switch(addr->sa_family)
    {
        case AF_INET:
            memcpy(&sin4, addr, sizeof(sin4));
            if(!inet_ntop(AF_INET, &sin4.sin_addr, 
                        out_buf, (socklen_t)size))
                return ADB_ERR_TOO_SMALL;
            break;

        case AF_INET6:
            memcpy(&sin6, addr, sizeof(sin6));
            if(!inet_ntop(AF_INET6, &sin6.sin6_addr, 
                        out_buf, (socklen_t)size))
                return ADB_ERR_TOO_SMALL;
            break;

        default:
            return ADB_ERR_UNSUPPORTED;
    }

    return ADB_ERR_OK;
}

uint16_t adb__sockaddr_get_port(
        const struct sockaddr *addr)
{
    struct sockaddr_in sin4 = {0};
    struct sockaddr_in6 sin6 = {0};
    if(!addr)
        return 0;

    switch(addr->sa_family)
    {
        case AF_INET:
            memcpy(&sin4, addr, sizeof(sin4));
            return ntohs(sin4.sin_port);

        case AF_INET6:
            memcpy(&sin6, addr, sizeof(sin6));
            return ntohs(sin6.sin6_port);

        default:
            return 0;
    }
}

