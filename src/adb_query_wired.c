#include <adb/adb_query.h>
#include "adb_query_priv.h"

#include <string.h>
#include <libusb.h>

#include "adb_log_priv.h"
#include "adb_alloc_priv.h"
#include "adb_ctx_priv.h"
#include "adb_lookup.h"

#define ADB__USB_INTERFACE_CLASS    0xFF
#define ADB__USB_INTERFACE_SUBCLASS 0x42
#define ADB__USB_INTERFACE_PROTOCOL 0x01
#define ADB__USB_DEVICE_CLASS       0xDC
#define ADB__USB_DEVICE_SUBCLASS    0x02

static bool adb__find_adb_interface(
        libusb_device *device,
        uint8_t *out_itf_idx,
        uint8_t *out_read_ep,
        uint8_t *out_write_ep)
{
    int res = 0;
    struct libusb_config_descriptor *config = NULL;

    if(!device || !out_itf_idx || !out_read_ep || !out_write_ep)
        return false;

    res = libusb_get_active_config_descriptor(device, &config);
    if(res != 0)
    {
        ADB__WARN("failed to get active USB configuration");
        ADB__DEBUG("reason: %s (%s)", libusb_error_name(res), libusb_strerror(res));
        return false;
    }

    for(uint8_t i = 0; i < config->bNumInterfaces; i++)
    {
        const struct libusb_interface *itf = &config->interface[i];
        const struct libusb_interface_descriptor *desc = NULL;
        uint8_t bulk_in = 0;
        uint8_t bulk_out = 0;
        bool found_in = false;
        bool found_out = false;

        if(itf->num_altsetting == 0)
            continue;
        desc = &itf->altsetting[0];

        if(desc->bInterfaceProtocol != ADB__USB_INTERFACE_PROTOCOL)
            continue;
        if(!((desc->bInterfaceClass == ADB__USB_INTERFACE_CLASS &&
              desc->bInterfaceSubClass == ADB__USB_INTERFACE_SUBCLASS) ||
             (desc->bInterfaceClass == ADB__USB_DEVICE_CLASS &&
              desc->bInterfaceSubClass == ADB__USB_DEVICE_SUBCLASS)))
            continue;

        for(int j = 0; j < desc->bNumEndpoints; j++)
        {
            const struct libusb_endpoint_descriptor *endpoint = &desc->endpoint[j];
            uint8_t direction = endpoint->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK;

            if((endpoint->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) != LIBUSB_TRANSFER_TYPE_BULK)
                continue;
            if(direction == LIBUSB_ENDPOINT_IN && !found_in)
            {
                found_in = true;
                bulk_in = endpoint->bEndpointAddress;
            }
            else if(direction == LIBUSB_ENDPOINT_OUT && !found_out)
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

static void adb__wired_info_clear(adb_wired_info_t *info)
{
    if(!info)
        return;
    if(info->device)
        libusb_unref_device(info->device);
    memset(info, 0, sizeof(*info));
}

static adb_error_t adb__wired_info_slot(
        adb_ctx_t *ctx,
        size_t index,
        adb_wired_info_t **out_info)
{
    adb_error_t res = ADB_ERR_OK;
    adb_wired_info_t *info = NULL;

    if(!ctx || !out_info)
        return ADB_ERR_PARAM;

    if(index >= ctx->wired_infos.size)
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
    }
    else
    {
        info = adb__dynarr_get(&ctx->wired_infos, adb_wired_info_t *, index);
        adb__wired_info_clear(info);
    }

    *out_info = info;
    return ADB_ERR_OK;
}

ADB__INLINE void adb__usb_get_string(
        libusb_device_handle *handle,
        uint8_t index,
        unsigned char *out,
        size_t out_size,
        uint16_t vendor_id,
        uint16_t product_id,
        const char *label)
{
    int res = 0;
    if(!handle || !index || !out || out_size == 0)
        return;

    res = libusb_get_string_descriptor_ascii(
            handle, index, out, (int)out_size);
    if(res < 0)
    {
        ADB__DEBUG("failed to read %s for %04X:%04X", 
                label, vendor_id, product_id);
        ADB__INFO("reason: %s (%s)", 
                libusb_error_name(res), libusb_strerror(res));
    }
}

static adb_error_t adb__append_conn_info(
        adb_ctx_t *ctx,
        libusb_device *device,
        const struct libusb_device_descriptor *desc,
        uint8_t itf_idx,
        uint8_t read_ep,
        uint8_t write_ep,
        size_t *total)
{
    adb_error_t res = ADB_ERR_OK;
    adb_wired_info_t *info = NULL;
    struct libusb_device_handle *handle = NULL;
    unsigned char manufacturer[256] = {0};
    unsigned char product[256] = {0};
    unsigned char serial[256] = {0};
    char manufacturer_tmp[256] = {0};
    char product_tmp[256] = {0};
    int usb_err = 0;

    if(!ctx || !device || !desc || !total)
        return ADB_ERR_PARAM;

    usb_err = libusb_open(device, &handle);
    if(usb_err != 0)
    {
        ADB__WARN("failed to open USB device %04X:%04X", 
                desc->idVendor, desc->idProduct);
        ADB__INFO("reason: %s (%s)", 
                libusb_error_name(usb_err), libusb_strerror(usb_err));
    }
    else
    {
        adb__usb_get_string(handle, desc->iManufacturer, 
                manufacturer, sizeof(manufacturer),
                desc->idVendor, desc->idProduct, "manufacturer");
        adb__usb_get_string(handle, desc->iProduct, 
                product, sizeof(product),
                desc->idVendor, desc->idProduct, "product");
        adb__usb_get_string(handle, desc->iSerialNumber, 
                serial, sizeof(serial),
                desc->idVendor, desc->idProduct, "serial");
        libusb_close(handle);
    }

    if(!manufacturer[0] || !product[0])
    {
        ADB__WARN("USB device contain no manufacturer/product name, using usb.ids database");
        if(adb__lookup_usb(desc->idVendor, desc->idProduct,
                manufacturer_tmp, sizeof(manufacturer_tmp),
                product_tmp, sizeof(product_tmp)))
        {
            ADB__INFO("found USB device inside usb.ids, using usb.ids "
                    "manufacturer/product name");
            if(!manufacturer[0])
                snprintf((char *)manufacturer, sizeof(manufacturer), 
                        "%s", manufacturer_tmp);
            if(!product[0])
                snprintf((char *)product, sizeof(product), "%s", product_tmp);
        } else {
            ADB__WARN("no USB device product was found in usb.ids database, "
                    "using libusb database");
        }
    }

    res = adb__wired_info_slot(ctx, *total, &info);
    if(res != ADB_ERR_OK)
        return res;

    info->device = libusb_ref_device(device);
    info->vendor_id = desc->idVendor;
    info->product_id = desc->idProduct;
    info->itf_idx = itf_idx;
    info->read_ep = read_ep;
    info->write_ep = write_ep;

    snprintf((char *)info->manufacturer, sizeof(info->manufacturer),
            "%s", manufacturer[0] ? (char *)manufacturer : "unknown");
    snprintf((char *)info->product, sizeof(info->product),
            "%s", product[0] ? (char *)product : "unknown");
    snprintf((char *)info->serial, sizeof(info->serial),
            "%s", serial[0] ? (char *)serial : "unknown");

    (*total)++;
    ADB__INFO("found ADB device %04X:%04X %s %s (%s)",
            info->vendor_id, info->product_id,
            info->manufacturer, info->product, info->serial);
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

    *out_infos = NULL;
    *out_info_count = 0;

    ADB__INFO("starting ADB wired device query");
    usb_count = libusb_get_device_list(ctx->usb, &list);
    if(usb_count < 0)
    {
        res = (int)usb_count;
        ADB__ERROR("failed to get USB device list: %s", 
                libusb_error_name(res));
        ADB__DEBUG("reason: %s (%s)", 
                libusb_error_name(res), libusb_strerror(res));
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

        if(!adb__find_adb_interface(device, &itf_idx, 
                    &read_ep, &write_ep))
            continue;

        ADB__DEBUG("ADB interface found on %04X:%04X "
                "(interface=%u, IN=0x%02X, OUT=0x%02X)",
                desc.idVendor, desc.idProduct,
                (unsigned int)itf_idx, read_ep, write_ep);

        adb_res = adb__append_conn_info(ctx, device, &desc,
                itf_idx, read_ep, write_ep, &total);
        if(adb_res != ADB_ERR_OK)
        {
            ADB__ERROR("failed to add ADB device %04X:%04X", 
                    desc.idVendor, desc.idProduct);
            ret = adb_res;
            break;
        }
    }

    libusb_free_device_list(list, true);
    *out_infos = ctx->wired_infos.data;
    *out_info_count = total;

    ADB__INFO("ADB device query completed: %zu device(s)", total);
    return ret;
}

