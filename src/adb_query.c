#include <adb/adb_query.h>
#include "adb_query_priv.h"

#include <stdio.h>
#include <stdbool.h>

#ifdef _WIN32
#   include <winsock2.h>
#   include <ws2tcpip.h>
#else
#   include <arpa/inet.h>
#endif

#include <libusb.h>
#include <mdns.h>

#include "adb_ctx_priv.h"
#include "adb_alloc_priv.h"
#include "adb_log_priv.h"
#include "adb_lookup.h"

#define ADB__USB_INTERFACE_CLASS    0xFF
#define ADB__USB_INTERFACE_SUBCLASS 0x42
#define ADB__USB_INTERFACE_PROTOCOL 0x01

#define ADB__USB_DEVICE_CLASS       0xDC
#define ADB__USB_DEVICE_SUBCLASS    0x02

#define ADB__MDNS_NAME_LENGTH_MAX   256
#define ADB__MDNS_BUFFER_SIZE       4096

typedef struct
{
    char hostname[ADB__MDNS_NAME_LENGTH_MAX];
    uint16_t port;

    struct sockaddr_in sin;
    struct sockaddr_in6 sin6;

    bool have_srv;
    bool have_ipv4;
    bool have_ipv6;
} adb__mdns_find_result_t;

static uint32_t adb__query_timeout_ms = 2000;

static bool adb__find_adb_interface(
        libusb_device *device,
        uint8_t *out_itf_idx,
        uint8_t *out_read_ep,
        uint8_t *out_write_ep)
{
    int res = 0;
    struct libusb_config_descriptor *config = NULL;
    uint8_t bulk_in = 0;
    uint8_t bulk_out = 0;

    if(!device || !out_itf_idx || !out_read_ep || !out_write_ep)
        return false;

    res = libusb_get_active_config_descriptor(device, &config);
    if(res != 0)
    {
        ADB__WARN("failed to get active USB configuration");
        ADB__DEBUG("reason: %s (%s)",
                libusb_error_name(res),
                libusb_strerror(res));
        return false;
    }

    for(uint8_t i = 0; i < config->bNumInterfaces; i++)
    {
        const struct libusb_interface *itf = &config->interface[i];
        const struct libusb_interface_descriptor *itf_desc = NULL;
        bool found_in = false;
        bool found_out = false;

        if(itf->num_altsetting == 0)
            continue;

        itf_desc = &itf->altsetting[0];

        if(!(itf_desc->bInterfaceProtocol == ADB__USB_INTERFACE_PROTOCOL &&
                (
                    (itf_desc->bInterfaceClass == ADB__USB_INTERFACE_CLASS &&
                     itf_desc->bInterfaceSubClass == ADB__USB_INTERFACE_SUBCLASS) ||
                    (itf_desc->bInterfaceClass == ADB__USB_DEVICE_CLASS &&
                     itf_desc->bInterfaceSubClass == ADB__USB_DEVICE_SUBCLASS)
                )))
        {
            continue;
        }

        for(int j = 0; j < itf_desc->bNumEndpoints; j++)
        {
            const struct libusb_endpoint_descriptor *endpoint =
                    &itf_desc->endpoint[j];

            if((endpoint->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK)
                    != LIBUSB_TRANSFER_TYPE_BULK)
                continue;

            if((endpoint->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK)
                    == LIBUSB_ENDPOINT_IN && !found_in)
            {
                found_in = true;
                bulk_in = endpoint->bEndpointAddress;
            }

            if((endpoint->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK)
                    == LIBUSB_ENDPOINT_OUT && !found_out)
            {
                found_out = true;
                bulk_out = endpoint->bEndpointAddress;
            }
        }

        if(found_in && found_out)
        {
            *out_itf_idx = i;
            *out_read_ep = bulk_in;
            *out_write_ep = bulk_out;

            libusb_free_config_descriptor(config);
            return true;
        }
    }

    libusb_free_config_descriptor(config);
    return false;
}

static adb_error_t adb__append_conn_info(
        adb_ctx_t *ctx,
        libusb_device *device,
        const struct libusb_device_descriptor *desc,
        const uint8_t itf_idx,
        const uint8_t read_ep,
        const uint8_t write_ep)
{
    adb_wired_info_t *conn_info = NULL;
    struct libusb_device_handle *handle = NULL;
    unsigned char manufacturer[256] = {0};
    unsigned char product[256] = {0};
    unsigned char serial[256] = {0};
    int res = 0;

    if(!ctx || !device || !desc)
        return ADB_ERR_PARAM;

    if(ctx->wired_info_count < ctx->infos_capacity)
    {
        conn_info = ctx->wired_infos[ctx->wired_info_count];
        if(conn_info)
        {
            ADB__DEBUG("reusing connection info slot %zu",
                    ctx->wired_info_count);
            memset(conn_info, 0, sizeof(*conn_info));
        }
        else
        {
            conn_info = adb__calloc(1, sizeof(*conn_info));
            if(!conn_info)
                return ADB_ERR_NO_MEM;
            ctx->wired_infos[ctx->wired_info_count] = conn_info;
        }
    }
    else
    {
        size_t old_capacity = 0;
        size_t new_capacity = 0;
        adb_wired_info_t **new_infos = NULL;

        old_capacity = ctx->infos_capacity;
        new_capacity = ctx->infos_capacity == 0
                ? 8 : ctx->infos_capacity * 2;

        new_infos = adb__realloc(
                ctx->wired_infos, new_capacity * sizeof(*new_infos));

        if(!new_infos)
            return ADB_ERR_NO_MEM;

        memset(new_infos + old_capacity, 0,
                (new_capacity - old_capacity) * sizeof(*new_infos));

        ctx->wired_infos = new_infos;
        ctx->infos_capacity = new_capacity;
        conn_info = adb__calloc(1, sizeof(*conn_info));
        if(!conn_info)
            return ADB_ERR_NO_MEM;

        ctx->wired_infos[ctx->wired_info_count] = conn_info;
        ADB__DEBUG("allocated connection info slot %zu",
                ctx->wired_info_count);
    }

    res = libusb_open(device, &handle);
    if(res != 0)
    {
        ADB__WARN("failed to open USB device %04X:%04X",
                desc->idVendor,
                desc->idProduct);
        ADB__INFO("reason: %s (%s)",
                    libusb_error_name(res),
                    libusb_strerror(res));
        return ADB_ERR_USB;
    }

    if(desc->iManufacturer)
    {
        res = libusb_get_string_descriptor_ascii(
                handle,
                desc->iManufacturer,
                manufacturer,
                sizeof(manufacturer));

        if(res < 0)
        {
            ADB__DEBUG("failed to read manufacturer for %04X:%04X",
                    desc->idVendor,
                    desc->idProduct);
            ADB__INFO("reason: %s (%s)",
                    libusb_error_name(res),
                    libusb_strerror(res));
        }
    }

    if(desc->iProduct)
    {
        res = libusb_get_string_descriptor_ascii(
                handle,
                desc->iProduct,
                product,
                sizeof(product));

        if(res < 0)
        {
            ADB__DEBUG("failed to read product for %04X:%04X",
                    desc->idVendor,
                    desc->idProduct);
            ADB__INFO("reason: %s (%s)",
                    libusb_error_name(res),
                    libusb_strerror(res));
        }
    }

    if(desc->iSerialNumber)
    {
        res = libusb_get_string_descriptor_ascii(
                handle,
                desc->iSerialNumber,
                serial,
                sizeof(serial));

        if(res < 0)
        {
            ADB__DEBUG("failed to read serial for %04X:%04X",
                    desc->idVendor,
                    desc->idProduct);
            ADB__INFO("reason: %s (%s)",
                    libusb_error_name(res),
                    libusb_strerror(res));
        }
    }

    libusb_close(handle);

    if(!manufacturer[0] || !product[0])
    {
        char manufacturer_tmp[sizeof(manufacturer)] = {0};
        char product_tmp[sizeof(product)] = {0};

        ADB__WARN("USB device contain no manufacturer/product name, "
                "using usb.ids database");
        if(adb__lookup_usb(
                conn_info->vendor_id, conn_info->product_id,
                manufacturer_tmp, sizeof(manufacturer_tmp),
                product_tmp, sizeof(product_tmp)))
        {
            ADB__INFO("found USB device inside usb.ids, using usb.ids "
                    "manufacturer/product name");
            memcpy(manufacturer, manufacturer_tmp, sizeof(manufacturer));
            memcpy(product, product_tmp, sizeof(product));
        } else {
            ADB__WARN("no USB device product was found "
                    "in usb.ids database, using libusb database");
        }
    }

    conn_info->device = libusb_ref_device(device);
    conn_info->vendor_id = desc->idVendor;
    conn_info->product_id = desc->idProduct;
    conn_info->itf_idx = itf_idx;
    conn_info->read_ep = read_ep;
    conn_info->write_ep = write_ep;

    snprintf((char *)conn_info->manufacturer, 
            sizeof(conn_info->manufacturer),
            "%s", manufacturer[0] ? (char *)manufacturer : "unknown");
    snprintf((char *)conn_info->product, sizeof(conn_info->product),
            "%s", product[0] ? (char *)product : "unknown");
    snprintf((char *)conn_info->serial, sizeof(conn_info->serial),
            "%s", serial[0] ? (char *)serial : "unknown");

    ctx->wired_info_count++;
    ADB__INFO("found ADB device %04X:%04X %s %s (%s)",
            conn_info->vendor_id,
            conn_info->product_id,
            conn_info->manufacturer,
            conn_info->product,
            conn_info->serial);

    return ADB_ERR_OK;
}

adb_error_t adb_query_wired(
        adb_ctx_t *ctx,
        adb_wired_info_t ***out_infos,
        size_t *out_info_count)
{
    adb_error_t ret = ADB_ERR_OK;
    int res = 0;
    libusb_device **list = NULL;
    ssize_t usb_count = 0;

    if(!ctx || !out_infos || !out_info_count)
        return ADB_ERR_PARAM;

    ADB__INFO("starting ADB wired device query");

    ctx->wired_info_count = 0;
    usb_count = libusb_get_device_list(ctx->usb, &list);
    if(usb_count < 0)
    {
        res = (int)usb_count;

        ADB__ERROR("failed to get USB device list: %s",
                libusb_error_name(res));
        ADB__DEBUG("reason: %s (%s)",
                libusb_error_name(res),
                libusb_strerror(res));

        return ADB_ERR_USB;
    }

    ADB__DEBUG("enumerated %zd USB device(s)", usb_count);
    for(ssize_t i = 0; i < usb_count; i++)
    {
        libusb_device *device = list[i];
        struct libusb_device_descriptor desc = {0};

        adb_error_t adb_res = ADB_ERR_OK;
        uint8_t itf_idx = 0;
        uint8_t read_ep = 0;
        uint8_t write_ep = 0;

        res = libusb_get_device_descriptor(device, &desc);
        if(res != 0)
        {
            ADB__WARN("failed to get USB device descriptor: %s",
                    libusb_error_name(res));
            continue;
        }

        if(desc.bDeviceClass != LIBUSB_CLASS_PER_INTERFACE)
            continue;

        if(!adb__find_adb_interface(
                device,
                &itf_idx,
                &read_ep,
                &write_ep))
            continue;

        ADB__DEBUG("ADB interface found on %04X:%04X "
                "(interface=%d, IN=0x%02X, OUT=0x%02X)",
                desc.idVendor,
                desc.idProduct,
                itf_idx,
                read_ep,
                write_ep);

        adb_res = adb__append_conn_info(ctx,
                device, &desc,
                itf_idx,
                read_ep, write_ep);
        if(adb_res != ADB_ERR_OK)
        {
            ADB__ERROR("failed to add ADB device %04X:%04X",
                    desc.idVendor,
                    desc.idProduct);
            ret = adb_res;
            break;
        }
    }

    libusb_free_device_list(list, true);

    *out_infos = ctx->wired_infos;
    *out_info_count = ctx->wired_info_count;

    ADB__INFO("ADB device query completed: %zu device(s)",
            ctx->wired_info_count);

    return ret;
}

const char *adb_wired_info_manufacturer(
        const adb_wired_info_t *info) {
    return info ? info->manufacturer : NULL;
}

const char *adb_wired_info_product(
        const adb_wired_info_t *info) {
    return info ? info->product : NULL;
}

void adb__wired_info_destroy(
        adb_wired_info_t *info)
{
    if(!info)
        return;
    libusb_unref_device(info->device);
    adb__free(info);
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
    adb__mdns_find_result_t *result = user_data;
    char name[ADB__MDNS_NAME_LENGTH_MAX] = {0};
    mdns_string_t record_name = {0};
    size_t offset = 0;

    (void)sock;
    (void)from;
    (void)addrlen;
    (void)entry;
    (void)query_id;
    (void)rclass;
    (void)ttl;
    (void)name_length;

    offset = name_offset;

    record_name = mdns_string_extract(
            data,
            size,
            &offset,
            name,
            sizeof(name));

    ADB__DEBUG(
            "record: name=\"%.*s\" type=%u",
            (int)record_name.length,
            record_name.str,
            rtype);

    if (rtype == MDNS_RECORDTYPE_SRV)
    {
        mdns_record_srv_t srv = {0};

        srv = mdns_record_parse_srv(
                data,
                size,
                record_offset,
                record_length,
                name,
                sizeof(name));

        if (srv.name.str && srv.name.length < sizeof(result->hostname))
        {
            memcpy(result->hostname, srv.name.str, srv.name.length);
            result->hostname[srv.name.length] = '\0';
            result->port = srv.port;
            result->have_srv = true;
        }

        ADB__DEBUG(
                "srv: target=\"%.*s\" port=%u priority=%u weight=%u",
                (int)srv.name.length,
                srv.name.str,
                srv.port,
                srv.priority,
                srv.weight);
    }
    else if (rtype == MDNS_RECORDTYPE_A)
    {
        struct sockaddr_in sin = {0};
        char address[INET_ADDRSTRLEN] = {0};
        size_t hostname_length = 0;

        mdns_record_parse_a(
                data,
                size,
                record_offset,
                record_length,
                &sin);

        inet_ntop(
                AF_INET,
                &sin.sin_addr,
                address,
                sizeof(address));

        hostname_length = strlen(result->hostname);

        if (result->have_srv &&
            record_name.length == hostname_length &&
            !memcmp(
                    record_name.str,
                    result->hostname,
                    hostname_length))
        {
            /*
             * mdns_record_parse_srv() returns port in host byte order.
             * sockaddr_in expects network byte order.
             */
            sin.sin_port = htons(result->port);
            result->sin = sin;
            result->have_ipv4 = true;
        }

        ADB__DEBUG(
                "a: address=\"%s\"",
                address);
    }
    else if (rtype == MDNS_RECORDTYPE_AAAA)
    {
        struct sockaddr_in6 sin6 = {0};
        char address[INET6_ADDRSTRLEN] = {0};
        size_t hostname_length = 0;

        mdns_record_parse_aaaa(
                data,
                size,
                record_offset,
                record_length,
                &sin6);

        inet_ntop(
                AF_INET6,
                &sin6.sin6_addr,
                address,
                sizeof(address));

        hostname_length = strlen(result->hostname);

        if (result->have_srv &&
            record_name.length == hostname_length &&
            !memcmp(
                    record_name.str,
                    result->hostname,
                    hostname_length))
        {
            /*
             * mdns_record_parse_srv() returns port in host byte order.
             * sockaddr_in6 expects network byte order.
             */
            sin6.sin6_port = htons(result->port);
            result->sin6 = sin6;
            result->have_ipv6 = true;
        }

        ADB__DEBUG(
                "aaaa: address=\"%s\"",
                address);
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

static adb_error_t adb__find_wireless_impl(
        const char *query_name,
        struct sockaddr *out)
{
    adb_error_t ret = ADB_ERR_OK;
    int sock = 0;
    int query_id = 0;
    size_t records = 0;
    uint8_t buffer[ADB__MDNS_BUFFER_SIZE] = {0};

    adb__mdns_find_result_t result = {0};
    mdns_query_t queries[2] = {0};

    if (!query_name || !out)
        return ADB_ERR_PARAM;

    out->sa_family = AF_UNSPEC;

    ADB__INFO("finding wireless device \"%s\"", query_name);
    sock = mdns_socket_open_ipv4(NULL);
    if (sock < 0)
    {
        adb__log_err_errno("failed to create mDNS socket");
        return ADB_ERR_NETWORK;
    }

    query_id = mdns_query_send(
            sock,
            MDNS_RECORDTYPE_SRV,
            query_name,
            strlen(query_name),
            buffer,
            sizeof(buffer),
            0);

    if (query_id < 0)
    {
        adb__log_err_errno("failed to send mDNS service query");
        ret = ADB_ERR_NETWORK;
        goto cleanup;
    }

    /*
     * mdns_query_recv() consumes one UDP datagram.
     * Keep receiving until we obtain the SRV record or timeout.
     */
    while (!result.have_srv)
    {
        ret = adb__mdns_timeout(sock, adb__query_timeout_ms);
        if (ret != ADB_ERR_OK)
            goto cleanup;

        records = mdns_query_recv(
                sock,
                buffer,
                sizeof(buffer),
                adb__mdns_record_callback,
                &result,
                query_id);

        if (records == 0)
            continue;
    }

    /*
     * Now resolve the SRV target hostname to A/AAAA.
     *
     * We do this as a separate query instead of relying on the
     * additional records that may happen to accompany the SRV
     * response.
     */
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

    if (query_id < 0)
    {
        adb__log_err_errno("failed to send mDNS address queries");
        ret = ADB_ERR_NETWORK;
        goto cleanup;
    }

    while (!result.have_ipv4 && !result.have_ipv6)
    {
        ret = adb__mdns_timeout(sock, adb__query_timeout_ms);
        if (ret != ADB_ERR_OK)
            goto cleanup;

        records = mdns_query_recv(
                sock,
                buffer,
                sizeof(buffer),
                adb__mdns_record_callback,
                &result,
                query_id);

        if (records == 0)
            continue;
    }

    if (result.have_ipv4)
    {
        memcpy(out, &result.sin, sizeof(result.sin));

        ADB__INFO("found wireless device \"%s\" successfully", query_name);
        goto cleanup;
    }

    if (result.have_ipv6)
    {
        memcpy(out, &result.sin6, sizeof(result.sin6));
        ADB__INFO("found wireless device \"%s\" successfully", query_name);
        goto cleanup;
    }

    ADB__ERROR("failed to get the appropriate IP for \"%s\"", result.hostname);
    /* explicitly not to uss any error here, even not found anything */

cleanup:
    mdns_socket_close(sock);
    return ret;
}

adb_error_t adb_query_wireless(
        adb_ctx_t *ctx,
        adb_wireless_info_t ***infos,
        size_t *info_count)
{
    if(!ctx || !infos || !info_count)
        return ADB_ERR_PARAM;
    return ADB_ERR_UNSUPPORTED;
}

adb_error_t adb_find_wireless(
        adb_ctx_t *ctx,
        const char *guid,
        adb_wireless_info_t **out_info)
{
    adb_error_t res = ADB_ERR_OK;
    int written = 0;
    char query_name[ADB__MDNS_NAME_LENGTH_MAX] = {0};
    adb_wireless_info_t tmp = {0};
    if(!ctx || !guid || !out_info)
        return ADB_ERR_PARAM;

    written = snprintf(
            query_name,
            sizeof(query_name),
            "%s._adb-tls-connect._tcp.local.",
            guid);
    if (written < 0 || written >= (int)sizeof(query_name))
    {
        ADB__ERROR("device GUID was too long to fit into buffer");
        ADB__INFO("GUID length: %lu bytes", strlen(guid));
        return ADB_ERR_GENERIC;
    }
    
    res = adb__find_wireless_impl(
            query_name, &tmp.addr);
    if(res != ADB_ERR_OK)
        return res;
    if(tmp.addr.sa_family == AF_UNSPEC)
        return ADB_ERR_OK;


    *out_info = adb__malloc(sizeof(tmp));
    if(!out_info)
        return ADB_ERR_NO_MEM;
    memcpy(*out_info, &tmp, sizeof(tmp));
    return ADB_ERR_OK;
}

adb_error_t adb_find_wireless_pairing(
        adb_ctx_t *ctx,
        const char *service_name,
        adb_wireless_info_t **out_info)
{
    adb_error_t res = ADB_ERR_OK;
    int written = 0;
    char query_name[ADB__MDNS_NAME_LENGTH_MAX] = {0};
    adb_wireless_info_t tmp = {0};
    if(!ctx || !service_name || !out_info)
        return ADB_ERR_PARAM;

    written = snprintf(
            query_name,
            sizeof(query_name),
            "%s._adb-tls-pairing._tcp.local.",
            service_name);
    if (written < 0 || written >= (int)sizeof(query_name))
    {
        ADB__ERROR("service name was too long to fit into buffer");
        ADB__INFO("service name length: %lu bytes", strlen(service_name));
        return ADB_ERR_GENERIC;
    }
    
    res = adb__find_wireless_impl(
            query_name, &tmp.addr);
    if(res != ADB_ERR_OK)
        return res;
    if(tmp.addr.sa_family == AF_UNSPEC)
        return ADB_ERR_OK;


    *out_info = adb__malloc(sizeof(tmp));
    if(!out_info)
        return ADB_ERR_NO_MEM;
    memcpy(*out_info, &tmp, sizeof(tmp));
    return ADB_ERR_OK;
}

adb_error_t adb_wireless_info_host(
        adb_wireless_info_t *info,
        char *out_buf,
        const size_t size)
{
    if(!info || !out_buf || size == 0)
        return ADB_ERR_PARAM;
    return adb__sockaddr_get_host(&info->addr, out_buf, size); 
}

uint16_t adb_wireless_info_port(
        adb_wireless_info_t *info)
{
    return info ? adb__sockaddr_get_port(&info->addr) : 0;
}

void adb_wireless_info_destroy(
        adb_wireless_info_t *info)
{
    if(!info)
        return;
    adb__free(info);
}
