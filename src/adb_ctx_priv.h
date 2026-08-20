#ifndef ADB_CTX_PRIV_H
#define ADB_CTX_PRIV_H

typedef struct libusb_context libusb_context;
typedef struct adb_ctx
{
    libusb_context *usb;
} adb_ctx_t;


#endif