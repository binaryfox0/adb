#include <adb/adb_query.h>
#include "adb_query_priv.h"

#include <stdbool.h>
#include <libusb.h>

#include "adb_ctx_priv.h"
#include "adb_alloc_priv.h"
#include "adb_log_priv.h"
#include "adb_lookup.h"

#define ADB__INTERFACE_CLASS    0xFF
#define ADB__INTERFACE_SUBCLASS 0x42
#define ADB__INTERFACE_PROTOCOL 0x01

#define ADB__DEVICE_CLASS       0xDC
#define ADB__DEVICE_SUBCLASS    0x02

static bool adb__find_adb_interface(
        libusb_device *device,
        uint8_t *itf_idx,
        uint8_t *read_ep,
        uint8_t *write_ep)
{
    int res = 0;
    struct libusb_config_descriptor *config = NULL;
    uint8_t bulk_in = 0;
    uint8_t bulk_out = 0;

    if(!device || !itf_idx || !read_ep || !write_ep)
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

        if(!(itf_desc->bInterfaceProtocol == ADB__INTERFACE_PROTOCOL &&
                (
                    (itf_desc->bInterfaceClass == ADB__INTERFACE_CLASS &&
                     itf_desc->bInterfaceSubClass == ADB__INTERFACE_SUBCLASS) ||
                    (itf_desc->bInterfaceClass == ADB__DEVICE_CLASS &&
                     itf_desc->bInterfaceSubClass == ADB__DEVICE_SUBCLASS)
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
            *itf_idx = i;
            *read_ep = bulk_in;
            *write_ep = bulk_out;

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

    snprintf((char *)conn_info->manufacturer, sizeof(conn_info->manufacturer),
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
        adb_wired_info_t ***infos,
        size_t *info_count)
{
    adb_error_t ret = ADB_ERR_OK;
    int res = 0;
    libusb_device **list = NULL;
    ssize_t usb_count = 0;

    if(!ctx || !infos || !info_count)
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

    *infos = ctx->wired_infos;
    *info_count = ctx->wired_info_count;

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

adb_error_t adb_query_wireless(
        adb_ctx_t *ctx,
        adb_wireless_info_t ***infos,
        size_t *info_count)
{
    if(!ctx || !infos || !info_count)
        return ADB_ERR_PARAM;
    return ADB_ERR_UNSUPPORTED;
}

void adb__wireless_info_destroy(
        adb_wireless_info_t *info)
{
    (void)info;
}
