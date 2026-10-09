#include <adb/adb_query.h>
#include "adb_query_priv.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#ifdef _WIN32
#   include <winsock2.h>
#   include <ws2tcpip.h>
#else
#   include <arpa/inet.h>
#endif

#include <libusb.h>

#include "adb_alloc_priv.h"

const char *adb_wired_info_manufacturer(
        const adb_wired_info_t *info) {
    return info ? info->manufacturer : NULL;
}

const char *adb_wired_info_product(
        const adb_wired_info_t *info) {
    return info ? info->product : NULL;
}

const char *adb_wired_info_serial(
        const adb_wired_info_t *info) {
    return info ? info->serial : NULL;
}

void adb__wired_info_destroy(
        adb_wired_info_t *info)
{
    if(!info)
        return;
    libusb_unref_device(info->device);
    adb__free(info);
}

adb_error_t adb_wireless_info_host(
        const adb_wireless_info_t *info,
        char *out_buf,
        const size_t size)
{
    if(!out_buf || size == 0 || 
            !adb__wireless_info_valid(info))
        return ADB_ERR_PARAM;
    return adb__sockaddr_host(
            info->have_ipv4 ? 
                (const struct sockaddr*)&info->sin :
                (const struct sockaddr*)&info->sin6, 
            out_buf, size); 
}

uint16_t adb_wireless_info_port(
        const adb_wireless_info_t *info)
{
    if(!adb__wireless_info_valid(info))
        return 0;
    return adb__sockaddr_port(
            info->have_ipv4 ? 
                (const struct sockaddr*)&info->sin :
                (const struct sockaddr*)&info->sin6);
}

adb_error_t adb_wireless_info_endpoint(
        const adb_wireless_info_t *info,
        char *out_buf,
        const size_t size)
{
    adb_error_t res = ADB_ERR_OK;
    char host[INET6_ADDRSTRLEN] = {0};
    uint16_t port = 0;
    int written = 0;

    if(!adb__wireless_info_valid(info) || !out_buf || size == 0)
        return ADB_ERR_PARAM;

    res = adb_wireless_info_host(info, host, sizeof(host));
    if(res != ADB_ERR_OK)
        return res;

    port = adb_wireless_info_port(info);

    if(strchr(host, ':'))
    {
        written = snprintf(
                out_buf,
                size,
                "[%s]:%u",
                host,
                (unsigned int)port);
    }
    else
    {
        written = snprintf(
                out_buf,
                size,
                "%s:%u",
                host,
                (unsigned int)port);
    }

    if(written < 0)
        return ADB_ERR_IO;

    if((size_t)written >= size)
        return ADB_ERR_TOO_SMALL;

    return ADB_ERR_OK;
}

void adb__wireless_info_destroy(
        adb_wireless_info_t *info)
{
    if(!info)
        return;
    adb__free(info);
}
