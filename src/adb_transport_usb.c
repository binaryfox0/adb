#include "adb_transport_usb.h"
#include "adb_transport.h"

#include <libusb.h>

#include "adb_alloc_priv.h"
#include "adb_log_priv.h"
#include "adb_error_priv.h"
#include "adb_query_priv.h"

typedef struct
{
    libusb_device_handle *handle;

    uint8_t read_ep;
    uint8_t write_ep;

    uint8_t interface_number;
} adb__usb_transport_t;


static adb_error_t adb__usb_read(
        void *userdata,
        void *buf,
        size_t size)
{
    adb__usb_transport_t *usb = userdata;

    int transferred = 0;
    int ret = 0;

    if(!usb || (!buf && size))
        return ADB_ERR_PARAM;

    if(size > INT_MAX)
        return ADB_ERR_PARAM;

    ret = libusb_bulk_transfer(
            usb->handle,
            usb->read_ep,
            buf,
            (int)size,
            &transferred,
            0);

    if(ret != LIBUSB_SUCCESS)
        return adb__error_from_libusb(ret);

    if(transferred != (int)size)
        return ADB_ERR_IO;

    return ADB_ERR_OK;
}


static adb_error_t adb__usb_write(
        void *userdata,
        const void *buf,
        size_t size)
{
    adb__usb_transport_t *usb = userdata;

    int transferred = 0;
    int ret = 0;

    if(!usb || (!buf && size))
        return ADB_ERR_PARAM;

    if(size > INT_MAX)
        return ADB_ERR_PARAM;

    ret = libusb_bulk_transfer(
            usb->handle,
            usb->write_ep,
            (unsigned char*)(uintptr_t)buf,
            (int)size,
            &transferred,
            0);

    if(ret != LIBUSB_SUCCESS)
        return adb__error_from_libusb(ret);

    if(transferred != (int)size)
        return ADB_ERR_IO;

    return ADB_ERR_OK;
}


static void adb__usb_destroy(
        void *userdata)
{
    adb__usb_transport_t *usb = userdata;

    if(!usb)
        return;

    if(usb->handle)
    {
        libusb_release_interface(
                usb->handle,
                usb->interface_number);

        libusb_close(usb->handle);
    }

    adb_free(usb);
}


adb_error_t adb__usb_transport_create(
        adb__transport_t *transport,
        const adb_wired_info_t *info)
{
    adb__usb_transport_t *usb = NULL;
    libusb_device_handle *handle = NULL;
    int ret = 0;

    if(!transport || !info)
        return ADB_ERR_PARAM;

    ADB__INFO("creating connection for wired device %04X:%04X %s %s",
            info->vendor_id, info->product_id,
            info->manufacturer, info->product);

    usb = adb__calloc(1, sizeof(*usb));
    if(!usb)
        return ADB_ERR_NO_MEM;

    ret = libusb_open(info->device, &handle);
    if(ret != LIBUSB_SUCCESS)
    {
        adb_free(usb);
        return adb__error_from_libusb(ret);
    }

    ret = libusb_claim_interface(
            handle,
            info->itf_idx);

    if(ret != LIBUSB_SUCCESS)
    {
        libusb_close(handle);
        adb_free(usb);
        return adb__error_from_libusb(ret);
    }

    usb->handle = handle;
    usb->read_ep = info->read_ep;
    usb->write_ep = info->write_ep;
    usb->interface_number = info->itf_idx;

    transport->userdata = usb;
    transport->read = adb__usb_read;
    transport->write = adb__usb_write;
    transport->destroy = adb__usb_destroy;

    ADB__INFO("created connection successfully for wired device "
             "%04X:%04X %s %s (interface %u)",
             info->vendor_id, info->product_id,
             info->manufacturer, info->product,
             info->itf_idx);

    return ADB_ERR_OK;
}
