#include "adb_query_priv.h"

#include <limits.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef _WIN32
#   include <ws2tcpip.h>
#else
#   include <arpa/inet.h>
#endif

#include "mdns.h"

#include "adb_alloc_priv.h"
#include "adb_log_priv.h"
#include "adb_utils.h"

#define ADB__MDNS_BUFFER_SIZE 4096

static uint32_t adb__query_timeout_ms = 2000;

typedef struct
{
    adb__dynarr_t *out;
    size_t *total;
    adb_error_t *out_err;
    void *send_buffer;
    size_t send_buffer_size;
} adb__mdns_cb_params_t;

static adb_wireless_info_t *adb__wireless_find_info(
        adb__dynarr_t *out,
        size_t total,
        const char *hostname,
        size_t hostname_len,
        uint16_t port)
{
    adb_wireless_info_t *info = NULL;
    size_t info_len = 0;

    if(!out || !hostname)
        return NULL;

    for(size_t i = 0; i < total; i++)
    {
        info = adb__dynarr_get(out, adb_wireless_info_t *, i);
        if(!info || !info->have_srv)
            continue;

        info_len = strlen(info->hostname);
        if(info_len == hostname_len &&
           !memcmp(info->hostname, hostname, hostname_len) &&
           info->sin.sin_port == htons(port))
            return info;
    }

    return NULL;
}

static adb_error_t adb__wireless_info_slot(
        adb__dynarr_t *out,
        size_t index,
        adb_wireless_info_t **out_info)
{
    adb_error_t res = ADB_ERR_OK;
    adb_wireless_info_t *info = NULL;

    if(!out || !out_info)
        return ADB_ERR_PARAM;

    if(index >= out->size)
    {
        info = adb__calloc(1, sizeof(*info));
        if(!info)
            return ADB_ERR_NO_MEM;

        res = adb__dynarr_push(out, &info);
        if(res != ADB_ERR_OK)
        {
            adb__free(info);
            return res;
        }
    }
    else
    {
        info = adb__dynarr_get(out, adb_wireless_info_t *, index);
        if(!info)
            return ADB_ERR_GENERIC;

        memset(info, 0, sizeof(*info));
    }

    *out_info = info;
    return ADB_ERR_OK;
}

static void adb__wireless_set_error(
        adb__mdns_cb_params_t *params,
        adb_error_t error)
{
    if(params && params->out_err)
        *params->out_err = error;
}

static bool adb__mdns_name_matches(
        const mdns_string_t *name,
        const char *hostname)
{
    size_t hostname_len = 0;

    if(!name || !name->str || !hostname)
        return false;

    hostname_len = strlen(hostname);
    return name->length == hostname_len &&
           !memcmp(name->str, hostname, hostname_len);
}

static void adb__wireless_apply_a(
        adb__dynarr_t *out,
        size_t total,
        const mdns_string_t *record_name,
        const struct sockaddr_in *sin)
{
    adb_wireless_info_t *info = NULL;

    if(!out || !record_name || !sin)
        return;

    for(size_t i = 0; i < total; i++)
    {
        info = adb__dynarr_get(out, adb_wireless_info_t *, i);
        if(!info || !info->have_srv || info->have_ipv4)
            continue;
        if(!adb__mdns_name_matches(record_name, info->hostname))
            continue;

        info->sin.sin_family = AF_INET;
        info->sin.sin_addr = sin->sin_addr;
        info->have_ipv4 = true;
    }
}

static void adb__wireless_apply_aaaa(
        adb__dynarr_t *out,
        size_t total,
        const mdns_string_t *record_name,
        const struct sockaddr_in6 *sin6)
{
    adb_wireless_info_t *info = NULL;

    if(!out || !record_name || !sin6)
        return;

    for(size_t i = 0; i < total; i++)
    {
        info = adb__dynarr_get(out, adb_wireless_info_t *, i);
        if(!info || !info->have_srv || info->have_ipv6)
            continue;
        if(!adb__mdns_name_matches(record_name, info->hostname))
            continue;

        info->sin6.sin6_family = AF_INET6;
        info->sin6.sin6_addr = sin6->sin6_addr;
        info->have_ipv6 = true;
    }
}

static int adb__mdns_record_callback(
        int sock,
        const struct sockaddr *from,
        size_t addrlen,
        mdns_entry_type_t entry,
        uint16_t query_id,
        uint16_t rtype,
        uint16_t rclass,
        uint32_t ttl,
        const void *data,
        size_t size,
        size_t name_offset,
        size_t name_length,
        size_t record_offset,
        size_t record_length,
        void *user_data)
{
    adb__mdns_cb_params_t *params = user_data;
    char name[ADB__MDNS_NAME_LENGTH_MAX] = {0};
    mdns_string_t record_name = {0};
    size_t offset = 0;

    (void)from;
    (void)addrlen;
    (void)entry;
    (void)query_id;
    (void)rclass;
    (void)ttl;
    (void)name_length;

    if(!params || !params->out || !params->total || !data)
        return 0;
    if(name_offset > size || record_offset > size ||
       record_length > size - record_offset)
        return 0;

    offset = name_offset;
    record_name = mdns_string_extract(
            data, size, &offset, name, sizeof(name));
    if(!record_name.str)
        return 0;

    ADB__DEBUG("record: name=\"%.*s\" type=%u",
            (int)record_name.length, record_name.str, (unsigned int)rtype);

    if(rtype == MDNS_RECORDTYPE_PTR)
    {
        mdns_string_t ptr = {0};
        int sent = 0;

        ptr = mdns_record_parse_ptr(data, size,
                record_offset, record_length, name, sizeof(name));
        if(!ptr.str || ptr.length == 0)
            return 0;

        if(ptr.length >= ADB__MDNS_NAME_LENGTH_MAX)
        {
            ADB__WARN("received mDNS service name is too long, skipping");
            ADB__INFO("name: \"%.*s\", len: %zu bytes",
                    MDNS_STRING_FORMAT(ptr), ptr.length);
            return 0;
        }

        if(!params->send_buffer || params->send_buffer_size == 0)
        {
            adb__wireless_set_error(params, ADB_ERR_PARAM);
            return 0;
        }

        sent = mdns_query_send(sock, MDNS_RECORDTYPE_SRV,
                ptr.str, ptr.length, params->send_buffer,
                params->send_buffer_size, 0);
        if(sent < 0)
        {
            adb__log_err_errno("failed to send mDNS SRV query");
            adb__wireless_set_error(params, ADB_ERR_NETWORK);
            return 0;
        }

        ADB__DEBUG("ptr=\"%.*s\" -> SRV query", MDNS_STRING_FORMAT(ptr));
        return 0;
    }

    if(rtype == MDNS_RECORDTYPE_SRV)
    {
        mdns_record_srv_t srv = {0};
        adb_wireless_info_t *info = NULL;
        adb_error_t res = ADB_ERR_OK;

        srv = mdns_record_parse_srv(data, size,
                record_offset, record_length, name, sizeof(name));
        if(!srv.name.str || srv.name.length == 0)
            return 0;

        if(srv.name.length >= ADB__MEMSZ(adb_wireless_info_t, hostname))
        {
            ADB__WARN("received mDNS service hostname is too long, skipping");
            ADB__INFO("name: \"%.*s\", len: %zu bytes",
                    MDNS_STRING_FORMAT(srv.name), srv.name.length);
            return 0;
        }

        info = adb__wireless_find_info(params->out, *params->total,
                srv.name.str, srv.name.length, srv.port);
        if(!info)
        {
            res = adb__wireless_info_slot(
                    params->out, *params->total, &info);
            if(res != ADB_ERR_OK)
            {
                adb__wireless_set_error(params, res);
                return 0;
            }
            (*params->total)++;
        }

        memcpy(info->hostname, srv.name.str, srv.name.length);
        info->hostname[srv.name.length] = '\0';
        info->sin.sin_port = htons(srv.port);
        info->sin6.sin6_port = htons(srv.port);
        info->have_srv = true;

        ADB__DEBUG("srv: target=\"%.*s\" port=%u priority=%u weight=%u",
                MDNS_STRING_FORMAT(srv.name), srv.port,
                srv.priority, srv.weight);
        return 0;
    }

    if(rtype == MDNS_RECORDTYPE_A)
    {
        struct sockaddr_in sin = {0};
        char address[INET_ADDRSTRLEN] = {0};

        if(record_length != 4)
            return 0;
        if(!mdns_record_parse_a(data, size,
                record_offset, record_length, &sin))
            return 0;

        adb__wireless_apply_a(params->out, *params->total,
                &record_name, &sin);
        (void)inet_ntop(AF_INET, &sin.sin_addr, address, sizeof(address));
        ADB__DEBUG("a: address=\"%s\"", address);
        return 0;
    }

    if(rtype == MDNS_RECORDTYPE_AAAA)
    {
        struct sockaddr_in6 sin6 = {0};
        char address[INET6_ADDRSTRLEN] = {0};

        if(record_length != 16)
            return 0;
        if(!mdns_record_parse_aaaa(data, size,
                record_offset, record_length, &sin6))
            return 0;

        adb__wireless_apply_aaaa(params->out, *params->total,
                &record_name, &sin6);
        (void)inet_ntop(AF_INET6, &sin6.sin6_addr,
                address, sizeof(address));
        ADB__DEBUG("aaaa: address=\"%s\"", address);
    }

    return 0;
}

static bool adb__wireless_all_valid(
        adb__dynarr_t *out,
        size_t total)
{
    adb_wireless_info_t *info = NULL;

    if(!out || total == 0)
        return false;

    for(size_t i = 0; i < total; i++)
    {
        info = adb__dynarr_get(out, adb_wireless_info_t *, i);
        if(!info || !adb__wireless_info_valid(info))
            return false;
    }

    return true;
}

static adb_error_t adb__mdns_recv_until(
        int sock,
        uint8_t *buffer,
        size_t buffer_size,
        adb__mdns_cb_params_t *params,
        uint64_t deadline)
{
    adb_error_t res = ADB_ERR_OK;
    uint64_t now = 0;
    uint64_t remaining64 = 0;
    uint32_t remaining_ms = 0;

    now = adb__util_monotonic_ms();
    if(now >= deadline)
        return ADB_ERR_TIMEOUT;

    remaining64 = deadline - now;
    if(remaining64 > (uint64_t)UINT32_MAX)
        remaining_ms = UINT32_MAX;
    else
        remaining_ms = (uint32_t)remaining64;

    res = adb__sock_timeout(sock, remaining_ms);
    if(res != ADB_ERR_OK)
        return res;

    (void)mdns_query_recv(sock, buffer, buffer_size,
            adb__mdns_record_callback, params, 0);

    if(params->out_err && *params->out_err != ADB_ERR_OK)
        return *params->out_err;

    return ADB_ERR_OK;
}

ADB__INLINE size_t adb__compact_winfo(
        adb__dynarr_t *winfos,
        const size_t total)
{
    adb_wireless_info_t **infos = NULL;
    adb_wireless_info_t *tmp = NULL;
    size_t left = 0;
    size_t right = 0;

    if(!winfos || !winfos->data || total > winfos->size)
        return 0;

    infos = winfos->data;
    right = total;

    while(left < right)
    {
        if(adb__wireless_info_valid(infos[left]))
        {
            left++;
            continue;
        }

        --right;
        tmp = infos[left];
        infos[left] = infos[right];
        infos[right] = tmp;
    }

    return left;
}

adb_error_t adb__query_mdns(
        const char *name,
        const size_t name_len,
        const bool find_mode,
        adb__dynarr_t *out,
        size_t *out_count)
{
    adb_error_t res = ADB_ERR_OK;
    adb_error_t timeout_res = ADB_ERR_OK;
    int sock = -1;
    uint8_t buffer[ADB__MDNS_BUFFER_SIZE] = {0};
    uint8_t send_buffer[ADB__MDNS_BUFFER_SIZE] = {0};
    size_t total = 0;
    size_t queried = 0;
    size_t hostname_len = 0;
    adb_wireless_info_t *info = NULL;
    adb__mdns_cb_params_t params = {0};
    mdns_query_t queries[2] = {0};
    uint64_t deadline = 0;
    uint64_t now = 0;

    if(!name || name_len == 0 || 
            name_len > (size_t)INT_MAX || !out || !out_count)
        return ADB_ERR_PARAM;

    *out_count = 0;
    sock = mdns_socket_open_ipv4(NULL);
    if(sock < 0)
    {
        adb__log_err_errno("failed to create mDNS socket");
        return ADB_ERR_NETWORK;
    }

    params.out = out;
    params.total = &total;
    params.out_err = &res;
    params.send_buffer = send_buffer;
    params.send_buffer_size = sizeof(send_buffer);

    if(mdns_query_send(sock,
                find_mode ? MDNS_RECORDTYPE_SRV : MDNS_RECORDTYPE_PTR,
                name, name_len, send_buffer, sizeof(send_buffer), 0) < 0)
    {
        adb__log_err_errno("failed to send mDNS query");
        res = ADB_ERR_NETWORK;
        goto fail;
    }

    ADB__INFO("finding wireless device \"%.*s\"", (int)name_len, name);
    deadline = adb__util_monotonic_ms() + adb__query_timeout_ms;
    for(;;)
    {
        now = adb__util_monotonic_ms();
        if(now >= deadline)
            break;

        timeout_res = adb__mdns_recv_until(
                sock, buffer, sizeof(buffer), &params, deadline);
        if(timeout_res == ADB_ERR_TIMEOUT)
            break;
        if(timeout_res != ADB_ERR_OK)
        {
            res = timeout_res;
            goto fail;
        }
    }

    if(total != 0)
    {
        deadline = adb__util_monotonic_ms() + adb__query_timeout_ms;
        for(;;)
        {
            while(queried < total)
            {
                info = adb__dynarr_get(out, adb_wireless_info_t *, queried);
                queried++;

                if(!info || !info->have_srv)
                    continue;

                hostname_len = strlen(info->hostname);
                if(hostname_len == 0)
                    continue;

                memset(queries, 0, sizeof(queries));
                queries[0].name = info->hostname;
                queries[0].length = hostname_len;
                queries[0].type = MDNS_RECORDTYPE_A;
                queries[1].name = info->hostname;
                queries[1].length = hostname_len;
                queries[1].type = MDNS_RECORDTYPE_AAAA;

                if(mdns_multiquery_send(sock, queries, 2,
                        send_buffer, sizeof(send_buffer), 0) < 0)
                {
                    adb__log_err_errno("failed to send mDNS address queries");
                    res = ADB_ERR_NETWORK;
                    goto fail;
                }
            }

            if(adb__wireless_all_valid(out, total))
                break;

            now = adb__util_monotonic_ms();
            if(now >= deadline)
                break;

            timeout_res = adb__mdns_recv_until(
                    sock, buffer, sizeof(buffer), &params, deadline);
            if(timeout_res == ADB_ERR_TIMEOUT)
                break;
            if(timeout_res != ADB_ERR_OK)
            {
                res = timeout_res;
                goto fail;
            }
        }
    }

    total = adb__compact_winfo(out, total);
    *out_count = total;
    mdns_socket_close(sock);

    ADB__INFO("wireless query completed: %zu device(s)", total);
    return ADB_ERR_OK;

fail:
    mdns_socket_close(sock);
    *out_count = 0;
    return res;
}
