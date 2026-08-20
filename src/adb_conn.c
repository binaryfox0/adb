#include <adb/adb_conn.h>

#include <stdbool.h>

#include <libusb.h>
#include "adb_log_priv.h"
#include "adb_ctx_priv.h"

#define ADB__INTERFACE_CLASS 0xFF
#define ADB__INTERFACE_SUBCLASS 0x42
#define ADB__INTERFACE_PROTOCOL 0x01

#define ADB__DEVICE_CLASS 0xDC
#define ADB__DEVICE_SUBCLASS 0x02

static bool adb__find_adb_interface(
        libusb_device *device,
        int *itf_idx,
        int *read_ep,
        int *write_ep)
{
    int res = 0;
    struct libusb_config_descriptor *config = NULL;
    int bulk_in = 0, bulk_out = 0;
    bool found_adb = false;

    res = libusb_get_active_config_descriptor(device, &config);
    if(res != 0)
        return false;

    for(int i = 0; i < config->bNumInterfaces; i++)
    {
        const struct libusb_interface *itf = NULL;
        const struct libusb_interface_descriptor *itf_desc = NULL;
        bool found_in = false, found_out = false;

        itf = &config->interface[i];
        if(itf->num_altsetting == 0)
        {
            continue;
        }

        itf_desc = &itf->altsetting[0];
        if(!(itf_desc->bInterfaceProtocol == ADB__INTERFACE_PROTOCOL && (
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
            found_adb = true;
            *itf_idx = i;
            *read_ep = bulk_in;
            *write_ep = bulk_out;
            break;
        }
    }

    libusb_free_config_descriptor(config);
    return found_adb;
}

adb_error_t adb_conn_query(
        adb_ctx_t *ctx,
        adb_conn_info_t ***infos,
        size_t *count)
{
    int res = 0;
    libusb_device **list = NULL;
    ssize_t usb_count = 0;
    if(!ctx || !infos || !count)
        return ADB_ERR_PARAM;

    usb_count = libusb_get_device_list(ctx->usb, &list);
    if(usb_count < 0)
    {
        ADB__ERROR("failed to get usb device list");
        ADB__INFO("reason: %s (%s)", 
                libusb_error_name(res), 
                libusb_strerror(res));
        return ADB_ERR_USB;
    }

    for(ssize_t i = 0; i < usb_count; i++)
    {
        libusb_device *device = list[i];
        struct libusb_device_descriptor desc = {0};

        int itf_idx = 0;
        int read_ep = 0, write_ep = 0;

        res = libusb_get_device_descriptor(device, &desc);
        if(res != 0)
        {
            ADB__ERROR("failed to get device descriptor");
            ADB__INFO("reason: %s (%s)", 
                    libusb_error_name(res), 
                    libusb_strerror(res));
            continue;
        }

        if(desc.bDeviceClass != LIBUSB_CLASS_PER_INTERFACE)
        {
            ADB__ERROR("skipped device with incorrect class");
            ADB__INFO("expected: 0x%02X, recieved: 0x%02X", 
                    LIBUSB_CLASS_PER_INTERFACE, desc.bDeviceClass);
            continue;
        }

        if(adb__find_adb_interface(
                device, &itf_idx, 
                &read_ep, &write_ep))
        {
            struct libusb_device_handle *handle = NULL;
            unsigned char manufacturer[256] = {0};
            unsigned char product[256] = {0};
            unsigned char serial[256] = {0};
            res = libusb_open(device, &handle);
            if(res != 0)
                continue;


            if (desc.iManufacturer) {
                libusb_get_string_descriptor_ascii(
                    handle,
                    desc.iManufacturer,
                    manufacturer,
                    sizeof(manufacturer));
            }

            if (desc.iProduct) {
                libusb_get_string_descriptor_ascii(
                    handle,
                    desc.iProduct,
                    product,
                    sizeof(product));
            }

            if (desc.iSerialNumber) {
                libusb_get_string_descriptor_ascii(
                    handle,
                    desc.iSerialNumber,
                    serial,
                    sizeof(serial));
            }

            ADB__INFO("manufacturer: \"%s\"", manufacturer[0] ? (char*)manufacturer : "unknown");
            ADB__INFO("product: \"%s\"", product[0] ? (char*)product : "unknown");
            ADB__INFO("serial: \"%s\"", serial[0] ? (char*)serial : "unknown");
            libusb_close(handle);
        }
    }

    libusb_free_device_list(list, true);
    return ADB_ERR_OK;
}