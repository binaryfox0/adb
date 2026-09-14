#include "adb_mdns.h"

#include <stdbool.h>

#ifdef _WIN32
#   include <winsock2.h>
#   include <ws2tcpip.h>
#else
#   include <arpa/inet.h>
#endif

#include <mdns.h>
#include "adb_log_priv.h"

#define ADB__MDNS_NAME_MAX 256
#define ADB__MDNS_BUFFER_SIZE 4096

#define ADB__MDNS_TIMEOUT_MS 2000

typedef struct
{
    char hostname[ADB__MDNS_NAME_MAX];
    uint16_t port;

    struct sockaddr_in sin;
    struct sockaddr_in6 sin6;

    bool have_srv;
    bool have_ipv4;
    bool have_ipv6;
} adb__mdns_find_result_t;

static int adb__mdns_record_callback(
        int sock, 
        const struct sockaddr* from, 
        size_t addrlen,
        mdns_entry_type_t entry, 
        uint16_t query_id, 
        uint16_t rtype,
        uint16_t rclass, 
        uint32_t ttl, 
        const void* data, 
        size_t size,
        size_t name_offset, 
        size_t name_length, 
        size_t record_offset,
        size_t record_length, 
        void* user_data)
{
    adb__mdns_find_result_t *result = user_data;
    char name[ADB__MDNS_NAME_MAX] = {0};
 
    (void)sock; 
    (void)from; 
    (void)addrlen;
    (void)entry; 
    (void)query_id; 
    (void)rtype;
    (void)rclass; 
    (void)ttl; 
    (void)data; 
    (void)size;
    (void)name_offset; 
    (void)name_length; 

    mdns_string_t record_name = {0};
    size_t offset = name_offset;

    record_name = mdns_string_extract(
            data, size,
            &offset,
            name, sizeof(name));

    ADB__DEBUG("record: name=\"%.*s\" type=%u",
              (int)record_name.length,
              record_name.str, rtype);

    if (rtype == MDNS_RECORDTYPE_SRV)
    {
        mdns_record_srv_t srv = mdns_record_parse_srv(
                data, size,
                record_offset, record_length,
                name, sizeof(name));
        if(srv.name.str && srv.name.length < sizeof(result->hostname))
        {
            memcpy(result->hostname, srv.name.str, srv.name.length);
            result->hostname[srv.name.length] = '\0';
            result->port = srv.port;
            result->have_srv = true;
        }

        ADB__DEBUG("srv: target=\"%.*s\" port=%u priority=%u weight=%u",
                  (int)srv.name.length, srv.name.str,
                  srv.port, srv.priority, srv.weight);
    }
    else if (rtype == MDNS_RECORDTYPE_A)
    {
        struct sockaddr_in sin = {0};
        char address[INET_ADDRSTRLEN] = {0};

        mdns_record_parse_a(
                data, size,
                record_offset, record_length,
                &sin);
            
        inet_ntop(AF_INET, &sin.sin_addr, 
                address, sizeof(address));

        if(result->have_srv && 
                record_name.length == strlen(result->hostname) &&
                !memcmp(record_name.str, result->hostname, 
                    record_name.length))
        {
            sin.sin_port = ntohs(result->port);
            result->sin = sin;
            result->have_ipv4 = true;
        }
        ADB__DEBUG("a: address=\"%s\"", address);
    }
    else if (rtype == MDNS_RECORDTYPE_AAAA)
    {
        struct sockaddr_in6 sin6 = {0};
        char address[INET6_ADDRSTRLEN] = {0};

        mdns_record_parse_aaaa(
                data, size,
                record_offset, record_length,
                &sin6);

        inet_ntop(AF_INET6, &sin6.sin6_addr, 
                address, sizeof(address));

        if(result->have_srv && 
                record_name.length == strlen(result->hostname) &&
                !memcmp(record_name.str, result->hostname, 
                    record_name.length))
        {
            sin6.sin6_port = ntohs(result->port);
            result->sin6 = sin6;
            result->have_ipv6 = true;
        }
        ADB__DEBUG("aaaa: address=\"%s\"", address);
    }

    return 0;
}

static adb_error_t adb__mdns_timeout(
        const int sock,
        const uint32_t timeout_ms)
{
    int err = 0;
    fd_set readfds;
    struct timeval timeout = {0};
    
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    err = select(
            sock + 1,
            &readfds,
            NULL,
            NULL,
            &timeout);
    if(err < 0)
    {
        adb__log_err_errno("failed to wait for mDNS response");
        return ADB_ERR_NETWORK;
    }
    if(err == 0)
    {
        ADB__ERROR("received no mDNS response for %ums", timeout_ms);
        return ADB_ERR_TIMEOUT;
    }
    return ADB_ERR_OK;
}

adb_error_t adb__mdns_find_service(
        const char *guid,
        struct sockaddr *out)
{
    adb_error_t ret = ADB_ERR_OK;
    char query_name[ADB__MDNS_NAME_MAX] = {0};
    int written = 0;
    int sock = 0;
    int query_id = 0;
    uint8_t buffer[ADB__MDNS_BUFFER_SIZE] = {0};

    adb__mdns_find_result_t result = {0};
    mdns_query_t queries[2] = {0};

    if(!guid || !out)
        return ADB_ERR_PARAM;

    ADB__INFO("finding wireless device \"%s\"", guid);

    written = snprintf(query_name, sizeof(query_name),
            "%s._adb-tls-connect._tcp.local.", guid);
    if(written >= (int)sizeof(query_name))
    {
        ADB__ERROR("device GUID was too long to fit into buffer");
        ADB__INFO("GUID length: %lu bytes", strlen(guid));
        return ADB_ERR_GENERIC;
    }

    sock = mdns_socket_open_ipv4(NULL);
    if(sock < 0)
    {
        adb__log_err_errno("failed to create mDNS socket");
        return ADB_ERR_NETWORK;
    }

    query_id = mdns_query_send(
            sock,
            MDNS_RECORDTYPE_SRV,
            query_name,
            (size_t)written,
            buffer,
            sizeof(buffer),
            0);
    if(query_id < 0)
    {
        adb__log_err_errno("failed to send mDNS service query");
        ret = ADB_ERR_NETWORK;
        goto cleanup;
    }

    ret = adb__mdns_timeout(sock, ADB__MDNS_TIMEOUT_MS);
    if(ret != ADB_ERR_OK)
        goto cleanup;

    mdns_query_recv(
            sock,
            buffer,
            sizeof(buffer),
            adb__mdns_record_callback,
            &result,
            query_id);

    if(!result.have_srv)
    {
        out->sa_family = AF_UNSPEC;
        goto cleanup;
    }

    if(result.have_ipv4 || result.have_ipv6)
    {
        if(result.have_ipv4)
            memcpy(out, &result.sin, sizeof(result.sin));
        else
            memcpy(out, &result.sin6, sizeof(result.sin6));

        ADB__INFO("found wireless device \"%s\" successfully", guid);
        goto cleanup;
    }

    queries[0].name = result.hostname;
    queries[1].name = result.hostname;
    queries[0].length = strlen(result.hostname);
    queries[1].length = strlen(result.hostname);
    queries[0].type = MDNS_RECORDTYPE_A;
    queries[1].type = MDNS_RECORDTYPE_AAAA;

    query_id = mdns_multiquery_send(
            sock,
            queries,
            2,
            buffer,
            sizeof(buffer),
            0);
    if(query_id < 0)
    {
        adb__log_err_errno("failed to send mDNS address queries");
        ret = ADB_ERR_NETWORK;
        goto cleanup;
    }
    
    ret = adb__mdns_timeout(sock, ADB__MDNS_TIMEOUT_MS);
    if(ret != ADB_ERR_OK)
        goto cleanup;

    /*
     * Do not zero the result as A/AAAA parsing depends on
     * the SRV hostname and the related flags.
     */
    mdns_query_recv(
            sock,
            buffer,
            sizeof(buffer),
            adb__mdns_record_callback,
            &result,
            query_id);

    if(result.have_ipv4 || result.have_ipv6)
    {
        if(result.have_ipv4)
            memcpy(out, &result.sin, sizeof(result.sin));
        else
            memcpy(out, &result.sin6, sizeof(result.sin6));

        ADB__INFO("found wireless device \"%s\" successfully", guid);
        goto cleanup;
    }

    ADB__ERROR("failed to get the appropriate IP for \"%s\"",
            result.hostname);

cleanup:
    mdns_socket_close(sock);
    return ret;
}
