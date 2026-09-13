#include "adb_transport_tcp.h"
#include "adb_transport.h"

#include <stdio.h>
#include <string.h>

#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "adb_alloc_priv.h"
#include "adb_error_priv.h"
#include "adb_log_priv.h"

static adb_error_t adb__tcp_read(
        void *userdata,
        void *buf,
        size_t size)
{
    int fd = (int)(uintptr_t)userdata;
    ssize_t ret = 0;
    if(fd < 0)
        return ADB_ERR_PARAM;

    if(!buf && size)
        return ADB_ERR_PARAM;

    ret = recv(
            fd,
            buf,
            size,
            0);

    if(ret < 0)
        return adb__error_from_errno(errno);

    if(ret == 0)
        return ADB_ERR_DISCONNECTED;

    if((size_t)ret != size)
        return ADB_ERR_IO;

    return ADB_ERR_OK;
}


static adb_error_t adb__tcp_write(
        void *userdata,
        const void *buf,
        size_t size)
{
    int fd = (int)(uintptr_t)userdata;
    ssize_t ret = 0;

    if(fd < 0)
        return ADB_ERR_PARAM;

    if(!buf && size)
        return ADB_ERR_PARAM;

    ret = send(
            fd,
            buf,
            size,
            0);

    if(ret < 0)
        return adb__error_from_errno(errno);

    if(ret == 0 && size != 0)
        return ADB_ERR_DISCONNECTED;

    if((size_t)ret != size)
        return ADB_ERR_IO;

    return ADB_ERR_OK;
}


static void adb__tcp_destroy(
        void *userdata)
{
    int fd = (int)(uintptr_t)userdata;
    if(fd >= 0)
    {
        shutdown(fd, SHUT_RDWR);
        close(fd);
    }
}

static socklen_t adb__addrlen_from_addr(
        const struct sockaddr *addr)
{
    switch(addr->sa_family)
    {
        case AF_INET: return sizeof(struct sockaddr_in);
        case AF_INET6: return sizeof(struct sockaddr_in6);
        default: return 0;
    }
}

static const char *adb__addr_to_string(
        const struct sockaddr *addr)
{
    static _Thread_local char buffer[INET6_ADDRSTRLEN + 8];
    char ip[INET6_ADDRSTRLEN] = {0};
    uint16_t port = 0;

    struct sockaddr_in sin4 = {0};
    struct sockaddr_in6 sin6 = {0};
    if(!addr)
        return NULL;

    switch(addr->sa_family)
    {
        case AF_INET:
            memcpy(&sin4, addr, sizeof(sin4));
            port = ntohs(sin4.sin_port);
            if(!inet_ntop(AF_INET, &sin4.sin_addr, 
                        ip, sizeof(ip)))
                return NULL;
            if(snprintf(buffer, sizeof(buffer), 
                        "%s:%u", ip, port) < 0)
                return NULL;
            break;

        case AF_INET6:
            memcpy(&sin6, addr, sizeof(sin6));
            port = ntohs(sin6.sin6_port);
            if(!inet_ntop(AF_INET6, &sin6.sin6_addr, 
                        ip, sizeof(ip)))
                return NULL;
            if(snprintf(buffer, sizeof(buffer), 
                        "[%s]:%u", ip, port) < 0)
                return NULL;
            break;

        default:
            return NULL;
    }

    return buffer;
}

adb_error_t adb__tcp_transport_create(
        adb__transport_t *transport,
        const struct sockaddr *addr)
{
    adb_error_t ret = ADB_ERR_OK;
    socklen_t addrlen = 0;
    int sock = 0;
    int err = 0;

    if(!transport || !addr)
        return ADB_ERR_PARAM;

    addrlen = adb__addrlen_from_addr(addr);
    if(addrlen == 0)
    {
        ADB__ERROR("unsupported socket type");
        ADB__INFO("got: %d", addr->sa_family);
        return ADB_ERR_UNSUPPORTED;
    }

    ADB__INFO("creating connection for wireless device %s",
            adb__addr_to_string(addr));

    sock = socket(
            addr->sa_family,
            SOCK_STREAM,
            0);

    if(sock < 0)
    {
        ADB__ERROR("failed to create socket for %s", adb__addr_to_string(addr));
        ADB__INFO("reason: %s", strerror(errno));
        ret = adb__error_from_errno(errno);
        goto fail;
    }

    err = connect(sock, addr, addrlen);
    if(err < 0)
    {
        ADB__ERROR("failed to connect to %s", adb__addr_to_string(addr));
        ADB__INFO("reason: %s", strerror(errno));
        ret = adb__error_from_errno(errno);
        goto fail;
    }

    transport->userdata = (void*)(uintptr_t)sock;
    transport->read = adb__tcp_read;
    transport->write = adb__tcp_write;
    transport->destroy = adb__tcp_destroy;

    ADB__INFO("created connection successfully for wireless device %s",
            adb__addr_to_string(addr));

    return ADB_ERR_OK;

fail:
    if(sock >= 0)
    {
        shutdown(sock, SHUT_RDWR);
        close(sock);
    }
    return ret;
}

bool adb__tcp_sockaddr_from_host_port(
        struct sockaddr *out,
        const char *host,
        const uint16_t port)
{
    int err = 0;
    struct sockaddr_in sin = {0};
    struct sockaddr_in6 sin6 = {0};
    if(!out || !host || port == 0)
        return false;

    ADB__INFO("parsing \"%s\" as IPv4", host);

    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    err = inet_pton(
            AF_INET,
            host,
            &sin.sin_addr);
    if(err == 1)
    {
        memcpy(out, &sin, sizeof(sin));
        ADB__INFO("parsed \"%s\" as IPv4 sucessfully", host);
        return true;
    }

    ADB__WARN("failed to parse \"%s\" as IPv4, parsing as IPv6", host);
    
    sin6.sin6_family = AF_INET6;
    sin6.sin6_port = htons(port);
    err = inet_pton(
            AF_INET6,
            host,
            &sin6.sin6_addr);
    if(err == 1)
    {
        memcpy(out, &sin6, sizeof(sin6));
        ADB__INFO("parsed \"%s\" as IPv6 sucessfully", host);
        return true;
    }

    ADB__ERROR("given host was not corresponding to IPv4 nor IPv6");
    return false;
}

