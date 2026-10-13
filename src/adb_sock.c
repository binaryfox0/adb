#include "adb_sock.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <sys/select.h>

#include "adb_log_priv.h"
#include "adb_utils.h"

adb_error_t adb__sockaddr_host(
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
            return ADB_ERR_UNIMPLEMENTED;
    }

    return ADB_ERR_OK;
}

uint16_t adb__sockaddr_port(
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

adb_error_t adb__sockaddr_endpoint(
        const struct sockaddr *addr,
        char *out_buf,
        const size_t buf_size)
{
    char host[INET6_ADDRSTRLEN] = {0};
    uint16_t port = 0;
    if(!addr || !out_buf || buf_size == 0)
        return ADB_ERR_PARAM;

    if(adb__sockaddr_host(addr, 
                host, sizeof(host)) != ADB_ERR_OK)
        return ADB_ERR_UNSUPPORTED;
    port = adb__sockaddr_port(addr);
    switch(addr->sa_family)
    {
        case AF_INET:
            if(snprintf(out_buf, buf_size, 
                        "%s:%u", host, port) < 0)
                return ADB_ERR_GENERIC;
            break;

        case AF_INET6:
            if(snprintf(out_buf, buf_size, 
                        "[%s]:%u", host, port) < 0)
                return ADB_ERR_GENERIC;
            break;

        default:
            return ADB_ERR_UNIMPLEMENTED;
    }
    return ADB_ERR_OK;
}

const char *adb__sockaddr_endpoint_local(
        const struct sockaddr *addr)
{
    static _Thread_local char buffer[INET6_ADDRSTRLEN + 8];
    memset(buffer, 0, sizeof(buffer));
    adb__sockaddr_endpoint(addr, buffer, sizeof(buffer));
    return buffer;
}

adb_error_t adb__sock_timeout(
        const int sock,
        const uint64_t timeout)
{
    uint64_t deadline = 0;

    deadline = adb__util_monotonic_ms() + timeout;

    for(;;)
    {
        uint64_t now = 0;
        uint64_t remain_ms = 0;
        fd_set readfds;
        struct timeval tv = {0};
        int err = 0;

        now = adb__util_monotonic_ms();
        if(now >= deadline)
            return ADB_ERR_TIMEOUT;

        remain_ms = deadline - now;

        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        tv.tv_sec = (long)(remain_ms / 1000ULL);
        tv.tv_usec = (long)((remain_ms % 1000ULL) * 1000ULL);

        err = select(
                sock + 1,
                &readfds,
                NULL,
                NULL,
                &tv);

        if(err > 0)
            return ADB_ERR_OK;

        if(err == 0)
            return ADB_ERR_TIMEOUT;

#ifdef _WIN32
        if(WSAGetLastError() == WSAEINTR)
            continue;
#else
        if(errno == EINTR)
            continue;
#endif

        adb__log_err_errno("failed to wait for mDNS response");
        return ADB_ERR_NETWORK;
    }
}
