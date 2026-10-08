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
#include <mdns.h>

#include "adb_ctx_priv.h"
#include "adb_alloc_priv.h"
#include "adb_log_priv.h"
#include "adb_lookup.h"
#include "adb_utils.h"

#define ADB__USB_INTERFACE_CLASS    0xFF
#define ADB__USB_INTERFACE_SUBCLASS 0x42
#define ADB__USB_INTERFACE_PROTOCOL 0x01

#define ADB__USB_DEVICE_CLASS       0xDC
#define ADB__USB_DEVICE_SUBCLASS    0x02

#define ADB__MDNS_BUFFER_SIZE       4096

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
        const uint8_t write_ep,
        size_t *total)
{
    adb_error_t res = ADB_ERR_OK;
    adb_wired_info_t *info = NULL;
    struct libusb_device_handle *handle = NULL;
    unsigned char manufacturer[256] = {0};
    unsigned char product[256] = {0};
    unsigned char serial[256] = {0};
    int usb_err = 0;

    if(!ctx || !device || !desc)
        return ADB_ERR_PARAM;


    usb_err = libusb_open(device, &handle);
    if(usb_err != 0)
    {
        ADB__WARN("failed to open USB device %04X:%04X",
                desc->idVendor,
                desc->idProduct);
        ADB__INFO("reason: %s (%s)",
                    libusb_error_name(usb_err),
                    libusb_strerror(usb_err));
        return ADB_ERR_USB;
    }

    if(desc->iManufacturer)
    {
        usb_err = libusb_get_string_descriptor_ascii(
                handle,
                desc->iManufacturer,
                manufacturer,
                sizeof(manufacturer));

        if(usb_err < 0)
        {
            ADB__DEBUG("failed to read manufacturer for %04X:%04X",
                    desc->idVendor,
                    desc->idProduct);
            ADB__INFO("reason: %s (%s)",
                    libusb_error_name(usb_err),
                    libusb_strerror(usb_err));
        }
    }

    if(desc->iProduct)
    {
        usb_err = libusb_get_string_descriptor_ascii(
                handle,
                desc->iProduct,
                product,
                sizeof(product));

        if(usb_err < 0)
        {
            ADB__DEBUG("failed to read product for %04X:%04X",
                    desc->idVendor,
                    desc->idProduct);
            ADB__INFO("reason: %s (%s)",
                    libusb_error_name(usb_err),
                    libusb_strerror(usb_err));
        }
    }

    if(desc->iSerialNumber)
    {
        usb_err = libusb_get_string_descriptor_ascii(
                handle,
                desc->iSerialNumber,
                serial,
                sizeof(serial));

        if(usb_err < 0)
        {
            ADB__DEBUG("failed to read serial for %04X:%04X",
                    desc->idVendor,
                    desc->idProduct);
            ADB__INFO("reason: %s (%s)",
                    libusb_error_name(usb_err),
                    libusb_strerror(usb_err));
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
                desc->idVendor, desc->idProduct,
                manufacturer_tmp, 
                sizeof(manufacturer_tmp),
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


    if(*total >= ctx->wired_infos.size)
    {
        info = adb__calloc(1, sizeof(*info));
        if(!info)
            return ADB_ERR_NO_MEM;
        res = adb__dynarr_push(&ctx->wired_infos, &info);
        if(res != ADB_ERR_OK)
        {
            adb__free(info);
            return res;
        }
    } else {
        info = adb__dynarr_get(&ctx->wired_infos, 
                adb_wired_info_t*, *total);
        // TODO: split into another func, e.g adb__wired_info_clear
        // without free-ing the pointer
        libusb_unref_device(info->device);
        memset(info, 0, sizeof(*info));
    }

    info->device = libusb_ref_device(device);
    info->vendor_id = desc->idVendor;
    info->product_id = desc->idProduct;
    info->itf_idx = itf_idx;
    info->read_ep = read_ep;
    info->write_ep = write_ep;

    snprintf((char *)info->manufacturer, 
            sizeof(info->manufacturer),
            "%s", manufacturer[0] ? (char *)manufacturer : "unknown");
    snprintf((char *)info->product, sizeof(info->product),
            "%s", product[0] ? (char *)product : "unknown");
    snprintf((char *)info->serial, sizeof(info->serial),
            "%s", serial[0] ? (char *)serial : "unknown");

    (*total)++;
    ADB__INFO("found ADB device %04X:%04X %s %s (%s)",
            info->vendor_id,
            info->product_id,
            info->manufacturer,
            info->product,
            info->serial);

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
    size_t total = 0;

    if(!ctx || !out_infos || !out_info_count)
        return ADB_ERR_PARAM;
    if((ctx->features & ADB__FEATURE_WIRED) == 0)
        return ADB_ERR_UNSUPPORTED;

    ADB__INFO("starting ADB wired device query");
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
                read_ep, write_ep,
                &total);
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

    *out_infos = ctx->wired_infos.data;
    *out_info_count = total;

    ADB__INFO("ADB device query completed: %zu device(s)",
            ctx->wired_infos.size);

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

ADB__INLINE bool adb__wireless_info_valid(
        const adb_wireless_info_t *info) {
    return info && (info->have_ipv4 || info->have_ipv6);
}

typedef struct
{
    bool is_single;
    size_t *total;
    adb_error_t *out_err;
    union 
    {
        adb_wireless_info_t *out_single;
        adb__dynarr_t *out_multi;
    };
} adb__mdns_cb_params_t;


static adb_error_t adb__query_wireless_impl(
        const char *query_name,
        const size_t name_len,
        const mdns_record_type_t query_type,
        adb__dynarr_t *out,
        size_t *out_count);
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
    bool is_single = false;
    size_t *total = NULL;
    adb__dynarr_t *out_multi = NULL;
    adb_wireless_info_t *out_single = NULL;

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

    is_single = params->is_single;
    total = params->total;
    out_multi = params->out_multi;
    out_single = params->out_single;

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
            (unsigned int)rtype);

    if(rtype == MDNS_RECORDTYPE_PTR)
    {
        mdns_string_t ptr = {0};
        ptr = mdns_record_parse_ptr(
                data, 
                size,
                record_offset,
                record_length,
                name,
                sizeof(name));

        if (ptr.str && ptr.length < 
                ADB__MEMSZ(adb_wireless_info_t, hostname))
        {
            adb_wireless_info_t *info = NULL;
            size_t count = 0;
            if(*total >= out_multi->size)
            {
                info = adb__calloc(1, sizeof(*info));
                if(!info)
                    return 0;

                if(adb__dynarr_push(out_multi, &info) != ADB_ERR_OK)
                {
                    adb__free(info);
                    *params->out_err = ADB_ERR_NO_MEM;
                    return 0;
                }
            } else {
                info = adb__dynarr_get(out_multi,
                        adb_wireless_info_t *, *total);
                // TODO: split into another func, e.g adb__wireless_info_clear
                // without free-ing the pointer
                memset(info, 0, sizeof(*info));
            }

            adb__query_wireless_impl(ptr.str, 
                    ptr.length,
                    MDNS_RECORDTYPE_SRV, 
                    out_multi, 
                    &count);
            (*total)++;
        } else {
            ADB__WARN("recieved mDNS service name is too long, skipping");
            ADB__INFO("name: \"%.*s\", len: %zu bytes", 
                    MDNS_STRING_FORMAT(ptr), ptr.length);
        }

        ADB__DEBUG("ptr=\"%.*s", MDNS_STRING_FORMAT(ptr));
    } else if(rtype == MDNS_RECORDTYPE_SRV) {
        mdns_record_srv_t srv = {0};
        srv = mdns_record_parse_srv(
                data,
                size,
                record_offset,
                record_length,
                name,
                sizeof(name));

        if (srv.name.str && srv.name.length < 
                ADB__MEMSZ(adb_wireless_info_t, hostname))
        {
            adb_wireless_info_t *info = NULL;
            if(*total >= out_multi->size)
            {
                info = adb__calloc(1, sizeof(*info));
                if(!info)
                    return 0;

                if(adb__dynarr_push(out_multi, &info) != ADB_ERR_OK)
                {
                    adb__free(info);
                    *params->out_err = ADB_ERR_NO_MEM;
                    return 0;
                }
            } else {
                info = adb__dynarr_get(out_multi,
                        adb_wireless_info_t *, *total);
                // TODO: split into another func, e.g adb__wireless_info_clear
                // without free-ing the pointer
                memset(info, 0, sizeof(*info));
            }

            memcpy(info->hostname, srv.name.str, srv.name.length);
            info->hostname[srv.name.length] = '\0';
            info->sin.sin_port = htons(srv.port);
            info->sin6.sin6_port = htons(srv.port);
            info->have_srv = true;
            (*total)++;
        } else {
            ADB__WARN("recieved mDNS service name is too long, skipping");
            ADB__INFO("name: \"%.*s\", len: %zu bytes", 
                    MDNS_STRING_FORMAT(srv.name), srv.name.length);
        }

        ADB__DEBUG("srv: target=\"%.*s\" port=%u priority=%u weight=%u",
                MDNS_STRING_FORMAT(srv.name),
                srv.port, srv.priority, srv.weight);
    }
    else if (rtype == MDNS_RECORDTYPE_A)
    {
        struct sockaddr_in sin = {0};
        char address[INET_ADDRSTRLEN] = {0};

        mdns_record_parse_a(
                data,
                size,
                record_offset,
                record_length,
                &sin);

        if(is_single)
        {
            out_single->sin = sin;
            out_single->have_ipv4 = true;
        } else {
            for(size_t i = 0; i < *total; i++)
            {
                adb_wireless_info_t *info = NULL;
                size_t hostname_len = 0;

                info = adb__dynarr_get(out_multi, adb_wireless_info_t*, i);
                if(info->have_ipv4)
                    continue;
                hostname_len = strlen(info->hostname);
                if(record_name.length == hostname_len &&
                        !memcmp(record_name.str, 
                           info->hostname, hostname_len))
                {
                    info->sin = sin;
                    info->have_ipv4 = true;
                    break;
                }
            }
        }
        
        inet_ntop(
                AF_INET,
                &sin.sin_addr,
                address,
                sizeof(address));

        ADB__DEBUG("a: address=\"%s\"", address);

    }
    else if (rtype == MDNS_RECORDTYPE_AAAA)
    {
        struct sockaddr_in6 sin6 = {0};
        char address[INET6_ADDRSTRLEN] = {0};

        mdns_record_parse_aaaa(
                data,
                size,
                record_offset,
                record_length,
                &sin6);

        if(is_single)
        {
            out_single->sin6 = sin6;
            out_single->have_ipv6 = true;
        } else {
            for(size_t i = 0; i < *total; i++)
            {
                adb_wireless_info_t *info = NULL;
                size_t hostname_len = 0;

                info = adb__dynarr_get(out_multi, adb_wireless_info_t*, i);
                if(info->have_ipv6)
                    continue;
                hostname_len = strlen(info->hostname);
                if(record_name.length == hostname_len &&
                        !memcmp(record_name.str, 
                           info->hostname, hostname_len))
                {
                    info->sin6 = sin6;
                    info->have_ipv6 = true;
                    break;
                }
            }
        }

        inet_ntop(
                AF_INET6,
                &sin6.sin6_addr,
                address,
                sizeof(address));

        ADB__DEBUG("aaaa: address=\"%s\"", address);
    }

    return 0;
}

static void adb__compact_winfo(
        adb__dynarr_t *winfos,
        const size_t total)
{
    adb_wireless_info_t **infos = NULL;
    adb_wireless_info_t *info = NULL;
    size_t left_ptr = 0;
    size_t right_ptr = 0;
    if(!winfos)
        return;

    right_ptr = total;
    infos = winfos->data;
    while(left_ptr < right_ptr)
    {
        while(left_ptr < right_ptr)
        {
            info = infos[left_ptr];
            if(info->have_ipv4 || info->have_ipv6)
                break;
            left_ptr++;
        }

        while(left_ptr < right_ptr)
        {
            info = infos[right_ptr - 1];
            if(info->have_ipv4 || info->have_ipv6)
                break;
            right_ptr--;
        }

        if(left_ptr < right_ptr)
        {
            adb_wireless_info_t *tmp_info = NULL;
            --right_ptr;
            tmp_info = infos[left_ptr];
            infos[left_ptr] = infos[right_ptr];
            infos[right_ptr] = tmp_info;
            left_ptr++;
        }
    }
}

static adb_error_t adb__query_wireless_impl(
        const char *name,
        const size_t name_len,
        const mdns_record_type_t query_type,
        adb__dynarr_t *out,
        size_t *out_count)
{
    adb_error_t res = ADB_ERR_OK;
    int sock = 0;
    uint8_t buffer[ADB__MDNS_BUFFER_SIZE] = {0};
    int query_id = 0;
    size_t written = 0;

    adb_wireless_info_t *info = NULL;
    size_t usable = 0;

    if (!name || !out || !out_count)
        return ADB_ERR_PARAM;

    ADB__INFO("finding wireless device \"%s\"", name);

    sock = mdns_socket_open_ipv4(NULL);
    if (sock < 0)
    {
        adb__log_err_errno("failed to create mDNS socket");
        return ADB_ERR_NETWORK;
    }

    query_id = mdns_query_send(
            sock,
            query_type,
            name,
            name_len,
            buffer,
            sizeof(buffer),
            0);

    if (query_id < 0)
    {
        adb__log_err_errno("failed to send mDNS service query");
        res = ADB_ERR_NETWORK;
        goto fail;
    }

    /*
     * mdns_query_recv() consumes one UDP datagram.
     * Keep receiving until we obtain the SRV record or timeout.
     */
    for(;;)
    {
        adb_error_t res2 = ADB_ERR_OK;
        size_t records = 0;

        res2 = adb__sock_timeout(sock, adb__query_timeout_ms);
        if(res2 == ADB_ERR_TIMEOUT)
            break;
        else if(res2 != ADB_ERR_OK) {
            res = res2;
            goto fail;
        }

        records = mdns_query_recv(
                sock,
                buffer,
                sizeof(buffer),
                adb__mdns_record_callback,
                &(adb__mdns_cb_params_t){
                    .is_single = false,
                    .total = &written,
                    .out_multi = out,
                    .out_err = &res
                }, query_id);

       
        if(res != ADB_ERR_OK)
            goto fail;

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
    adb__dynarr_foreach_rev(out, adb_wireless_info_t*, info)
    {
        mdns_query_t queries[2] = {0};
        size_t hostname_len = 0;
        size_t records = 0;

        if(adb__wireless_info_valid(info))
            continue;

        hostname_len = strlen(info->hostname);
        queries[0].name = info->hostname;
        queries[0].length = hostname_len;
        queries[0].type = MDNS_RECORDTYPE_A;

        queries[1].name = info->hostname;
        queries[1].length = hostname_len;
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
            res = ADB_ERR_NETWORK;
            goto fail;
        }

        while(!info->have_ipv4 && !info->have_ipv6)
        {
            adb_error_t res2 = ADB_ERR_OK;
            res2 = adb__sock_timeout(sock, adb__query_timeout_ms);
            if(res2 == ADB_ERR_TIMEOUT)
                break;
            else if(res2 != ADB_ERR_OK) {
                res = res2;
                goto fail;
            }

            records = mdns_query_recv(
                    sock,
                    buffer,
                    sizeof(buffer),
                    adb__mdns_record_callback,
                    &(adb__mdns_cb_params_t){
                        .is_single = true,
                        .total = &written,
                        .out_single = info,
                        .out_err = &res
                    },
                    query_id);

            /* unlikely to fail */
            if(res != ADB_ERR_OK)
                goto fail;

            if (records == 0)
                continue;
        }
    }

    adb__compact_winfo(out, written);
    adb__dynarr_foreach(out, adb_wireless_info_t*, info)
    {
        if(!adb__wireless_info_valid(info))
            break;
        usable++;
    }

    *out_count = usable;
    mdns_socket_close(sock);
    return ADB_ERR_OK;

fail:
    mdns_socket_close(sock);
    return res;
}

adb_error_t adb_query_wireless(
        adb_ctx_t *ctx,
        adb_wireless_info_t ***infos,
        size_t *info_count)
{
    adb_error_t res = ADB_ERR_OK;
    if(!ctx || !infos || !info_count)
        return ADB_ERR_PARAM;

    res = adb__query_wireless_impl(
            "_adb-tls-connect._tcp.local.", 
            MDNS_RECORDTYPE_PTR,
            &ctx->wireless_infos, &ctx->wireless_info_count);
    if(res == ADB_ERR_OK)
    {
        *infos = ctx->wireless_infos.data;
        *info_count = ctx->wireless_info_count;
    }
    return res;
}

adb_error_t adb_find_wireless(
        adb_ctx_t *ctx,
        const char *guid,
        adb_wireless_info_t **out_info)
{
    adb_error_t res = ADB_ERR_OK;
    int written = 0;
    char query_name[ADB__MDNS_NAME_LENGTH_MAX] = {0};
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
        ADB__INFO("GUID length: %zu bytes", strlen(guid));
        return ADB_ERR_GENERIC;
    }
    
    res = adb__query_wireless_impl(query_name, MDNS_RECORDTYPE_SRV,
            &ctx->dev_winfos, &ctx->dev_winfo_count);
    if(res != ADB_ERR_OK)
        return res;

    if(ctx->dev_winfo_count == 0)
        return ADB_ERR_NOT_FOUND;
    if(ctx->dev_winfo_count >= 1)
        return ADB_ERR_AMBIGUOUS;
    if(res == ADB_ERR_OK)
        *out_info = adb__dynarr_get(&ctx->dev_winfos, adb_wireless_info_t*, 0);
    return res;
}

adb_error_t adb_find_wireless_pairing(
        adb_ctx_t *ctx,
        const char *service_name,
        adb_wireless_info_t **out_info)
{
    adb_error_t res = ADB_ERR_OK;
    int written = 0;
    char query_name[ADB__MDNS_NAME_LENGTH_MAX] = {0};
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
    
    res = adb__query_wireless_impl(query_name, MDNS_RECORDTYPE_SRV,
            &ctx->pair_winfos, &ctx->pair_winfo_count);
    if(res != ADB_ERR_OK)
        return res;

    if(ctx->pair_winfo_count == 0)
        return ADB_ERR_NOT_FOUND;
    if(ctx->pair_winfo_count >= 1)
        return ADB_ERR_AMBIGUOUS;
    if(res == ADB_ERR_OK)
        *out_info = adb__dynarr_get(&ctx->pair_winfos, adb_wireless_info_t*, 0);
    return res;
}

adb_error_t adb_wireless_info_host(
        const adb_wireless_info_t *info,
        char *out_buf,
        const size_t size)
{
    if(!out_buf || size == 0 || 
            adb__wireless_info_valid(info))
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
