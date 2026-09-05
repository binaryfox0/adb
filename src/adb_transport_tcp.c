#include "adb_transport_tcp.h"
#include "adb_transport.h"

#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "adb_alloc_priv.h"
#include "adb_error_priv.h"

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


adb_error_t adb__tcp_transport_create(
        adb__transport_t *transport,
        const char *host,
        const uint16_t port)
{
    adb_error_t ret = ADB_ERR_OK;
    int sock = 0;
    struct sockaddr_in addr = {0};
    int err = 0;
    if(!transport || !host || port == 0)
        return ADB_ERR_PARAM;

    sock = socket(
            AF_INET,
            SOCK_STREAM,
            0);

    if(sock < 0)
    {
        ret = adb__error_from_errno(errno);
        goto fail;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    err = inet_pton(
            AF_INET,
            host,
            &addr.sin_addr);

    if(err != 1)
    {
        err = ADB_ERR_NETWORK;
        goto fail;
    }

    err = connect(
            sock,
            (struct sockaddr *)&addr,
            sizeof(addr));

    if(err < 0)
    {
        ret = adb__error_from_errno(errno);
        goto fail;
    }

    transport->userdata = (void*)(uintptr_t)sock;
    transport->read = adb__tcp_read;
    transport->write = adb__tcp_write;
    transport->destroy = adb__tcp_destroy;

    return ADB_ERR_OK;

fail:
    if(sock >= 0)
    {
        shutdown(sock, SHUT_RDWR);
        close(sock);
    }
    return ret;
}
