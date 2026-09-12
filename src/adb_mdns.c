#include "adb_mdns.h"

#include <mdns.h>
#include "adb_log_priv.h"

#define ADB__MDNS_NAME_MAX 256
#define ADB__MDNS_BUFFER_SIZE 2048

#define ADB__MDNS_TIMEOUT_MS 2000

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
    mdns_record_srv_t srv = {0};
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
    (void)record_offset;
    (void)record_length; 
    (void)user_data;

    if(rtype != MDNS_RECORDTYPE_SRV)
        return 0;
    
    srv = mdns_record_parse_srv(
            data, size, 
            record_offset, record_length, 
            name, sizeof(name));
    ADB__INFO("%s", name);
    (void)srv;
    return 0;
}


adb_error_t adb__mdns_find_service(
        const char *guid,
        adb__mdns_info_t *out)
{
    adb_error_t ret = ADB_ERR_OK;
    char query_name[ADB__MDNS_NAME_MAX] = {0};
    int written = 0;
    int sock = 0;
    int query_id = 0;
    uint8_t buffer[ADB__MDNS_BUFFER_SIZE] = {0};

    fd_set readfds;
    struct timeval timeout = {0};
    int err = 0;
(void)out;
    if(!guid)
        return ADB_ERR_PARAM;

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
        adb__log_err_errno("failed to send mDNS query");
        ret = ADB_ERR_NETWORK;
        goto fail;
    }

    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);

    timeout.tv_sec = ADB__MDNS_TIMEOUT_MS / 1000;
    timeout.tv_usec = (ADB__MDNS_TIMEOUT_MS % 1000) * 1000;

    err = select(
            sock + 1, 
            &readfds, 
            NULL, 
            NULL, 
            &timeout);
    if(err < 0)
    {
        adb__log_err_errno("failed to set %dms timeout for mDNS",
                ADB__MDNS_TIMEOUT_MS);
        ret = ADB_ERR_NETWORK;
        goto fail;
    }
    if(err == 0)
    {
        ADB__ERROR("recieved no mDNS response for %dms",
                ADB__MDNS_TIMEOUT_MS);
        ret = ADB_ERR_TIMEOUT;
        goto fail;
    }

    mdns_query_recv(
            sock,
            buffer,
            sizeof(buffer),
            adb__mdns_record_callback,
            out,
            query_id);

fail:
    mdns_socket_close(sock);
    return ret;
}
