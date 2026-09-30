#include "adb_transport_tcp.h"
#include "adb_transport.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <poll.h>

#include "adb_alloc_priv.h"
#include "adb_error_priv.h"
#include "adb_log_priv.h"
#include "adb_queue.h"
#include "adb_utils.h"

#define ADB__TCP_KEEPALIVE_INTERVAL 1
#define ADB__TCP_KEEPCNT 10

typedef struct
{
    int fd;
    adb__queue_t queue;
} adb__tcp_transport_t;

static adb_error_t adb__tcp_read(
        void *userdata,
        void *buf,
        size_t size)
{
    int fd = ((adb__tcp_transport_t*)userdata)->fd;
    uint8_t *data = buf;
    size_t offset = 0;
    ssize_t ret = 0;

    if(fd < 0)
        return ADB_ERR_PARAM;

    if(!buf && size)
        return ADB_ERR_PARAM;

    while(offset < size)
    {
        ret = recv(fd, data + offset, size - offset, 0);
        if(ret < 0)
            return adb__error_from_errno(errno);
        if(ret == 0)
            return ADB_ERR_DISCONNECTED;

        offset += (size_t)ret;
    }

    return ADB_ERR_OK;
}

/*
 * Read with timeout, with the timeout applied for all transfers,
 * not per each transfer. Using a queue to queue data if it 
 * does not reach the amount of data we need 
 */
static adb_error_t adb__tcp_read_timeout(
        void *userdata,
        void *buf,
        const size_t size,
        const uint32_t timeout_ms)
{
    int fd = 0;
    adb__queue_t *queue = NULL;
    uint64_t deadline = 0;
    struct pollfd pfd = {0};

    if(!userdata || !buf || size == 0)
        return ADB_ERR_PARAM;
    
    fd = ((adb__tcp_transport_t*)userdata)->fd;
    queue = &((adb__tcp_transport_t*)userdata)->queue;
    deadline = adb__util_monotonic_ms() + timeout_ms;
    pfd.fd = fd;
    pfd.events = POLLIN;

    for(;;)
    {
        uint64_t now = 0;
        uint64_t remaining = 0;
        int poll_res = 0;
        ssize_t read_res = 0;
        uint8_t tmp[4096] = {0};

        if(queue->size >= size)
            return adb__queue_pop(queue, buf, size);

        now = adb__util_monotonic_ms();
        if(now >= deadline)
            return ADB_ERR_TIMEOUT;

        remaining = deadline - now;
        poll_res = poll(&pfd, 1, 
                ADB__MIN((int)remaining, INT_MAX));
        if(poll_res < 0)
        {
            if(errno == EINTR)
                continue;
            return ADB_ERR_IO;
        }

        if(poll_res == 0)
            return ADB_ERR_TIMEOUT;

        if((pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0)
            return ADB_ERR_IO;

        read_res = recv(fd, tmp, sizeof(tmp), 0);
        if(read_res > 0)
        {
            adb_error_t res = adb__queue_push(
                    queue, 
                    tmp, (size_t)read_res);
            if(res != ADB_ERR_OK)
                return res;
            continue;
        }

        if(read_res == 0)
            return ADB_ERR_DISCONNECTED;
        if(errno == EINTR)
            continue;

        return ADB_ERR_IO;
    }
}

static adb_error_t adb__tcp_write(
        void *userdata,
        const void *buf,
        const size_t size)
{
    int fd = ((adb__tcp_transport_t*)userdata)->fd;
    const uint8_t *data = buf;
    size_t offset = 0;
    ssize_t ret = 0;

    if(fd < 0)
        return ADB_ERR_PARAM;

    if(!buf && size)
        return ADB_ERR_PARAM;

    while(offset < size)
    {
        ret = send(fd, data + offset, size - offset, 0);
        if(ret < 0)
            return adb__error_from_errno(errno);
        if(ret == 0)
            return ADB_ERR_DISCONNECTED;

        offset += (size_t)ret;
    }

    return ADB_ERR_OK;
}


static void adb__tcp_destroy_fd(
        const int fd);
static void adb__tcp_destroy(
        void *userdata)
{
    adb__tcp_transport_t *tcp = userdata;
    if(!tcp)
        return;

    adb__tcp_destroy_fd(tcp->fd);
    adb__queue_destroy(&tcp->queue);
    adb__free(tcp);
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
    char host[INET6_ADDRSTRLEN] = {0};
    uint16_t port = 0;
    if(!addr)
        return NULL;

    if(adb__sockaddr_get_host(addr, 
                host, sizeof(host)) != ADB_ERR_OK)
        return NULL;
    port = adb__sockaddr_get_port(addr);
    switch(addr->sa_family)
    {
        case AF_INET:
            if(snprintf(buffer, sizeof(buffer), 
                        "%s:%u", host, port) < 0)
                return NULL;
            break;

        case AF_INET6:
            if(snprintf(buffer, sizeof(buffer), 
                        "[%s]:%u", host, port) < 0)
                return NULL;
            break;

        default:
            return NULL;
    }

    return buffer;
}

static bool adb__tcp_set_sockopts(
        const int sock)
{
    if(setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, 
            (int[1]){1}, sizeof(int)) < 0)
        goto fail;

    if(setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE,
                (int[1]){ADB__TCP_KEEPALIVE_INTERVAL}, 
                sizeof(int)) < 0)
        goto fail;
    
    if(setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL,
                (int[1]){ADB__TCP_KEEPALIVE_INTERVAL}, 
                sizeof(int)) < 0)
        goto fail;
    
    if(setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT,
                (int[1]){ADB__TCP_KEEPCNT}, 
                sizeof(int)) < 0)
        goto fail;

    return true;

fail:
    adb__log_err_errno("failed to set options for tcp socket");
    return false;
}

static adb_error_t adb__tcp_create_fd(
        const struct sockaddr *addr,
        int *out_fd)
{
    adb_error_t res = ADB_ERR_OK;
    socklen_t addrlen = 0;
    int sock = -1;
    int err = 0;
    
    if(!addr || !out_fd)
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
        adb__log_err_errno("failed to create socket for %s", 
                adb__addr_to_string(addr));
        res = adb__error_from_errno(errno);
        goto fail;
    }

    if(!adb__tcp_set_sockopts(sock))
    {
        res = ADB_ERR_NETWORK;
        goto fail;
    }

    err = connect(sock, addr, addrlen);
    if(err < 0)
    {
        adb__log_err_errno("failed to connect to %s", 
                adb__addr_to_string(addr));
        res = adb__error_from_errno(errno);
        goto fail;
    }

    *out_fd = sock;
    return ADB_ERR_OK;

fail:
    adb__tcp_destroy_fd(sock);
    return res;
}

static void adb__tcp_destroy_fd(
        const int fd)
{
    if(fd >= 0)
    {
        shutdown(fd, SHUT_RDWR);
        close(fd);
    }
}

adb_error_t adb__tcp_transport_create(
        adb__transport_t *transport,
        const struct sockaddr *addr)
{
    adb_error_t res = ADB_ERR_OK;
    adb__tcp_transport_t *tcp = NULL;
    if(!transport || !addr)
        return ADB_ERR_PARAM;

    tcp = adb__calloc(1, sizeof(*tcp));
    if(!tcp)
        return ADB_ERR_NO_MEM;

    res = adb__tcp_create_fd(addr, &tcp->fd);
    if(res != ADB_ERR_OK)
        goto fail;

    transport->userdata = tcp;
    transport->read = adb__tcp_read;
    transport->read_timeout = adb__tcp_read_timeout;
    transport->write = adb__tcp_write;
    transport->destroy = adb__tcp_destroy;

    ADB__INFO("created connection successfully for wireless device %s",
            adb__addr_to_string(addr));

    return ADB_ERR_OK;

fail:
    adb__tcp_destroy(tcp);
    return res;
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

