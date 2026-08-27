#include <adb/adb_error.h>
#include "adb_error_priv.h"

#include <errno.h>
#include <libusb.h>

const char *adb_strerror(
        const adb_error_t err)
{
    static const char *error_msgs[ADB__ERR_COUNT] =
    {
        [ADB_ERR_OK]        = "no error",
        [ADB_ERR_GENERIC]   = "generic error",
        [ADB_ERR_PARAM]     = "paramaters error",
        [ADB_ERR_NO_MEM]    = "out of memory",
        [ADB_ERR_USB]       = "usb error",
        [ADB_ERR_NETWORK]   = "network error"
    };
    if(err < 0 || err >= ADB__ERR_COUNT)
        return "unknown error";

    return error_msgs[err] ? 
            error_msgs[err] : "unassigned error";
}

adb_error_t adb__error_from_errno(
        const int error)
{
    switch(error)
    {
        case ETIMEDOUT:
        case EAGAIN:
            return ADB_ERR_TIMEOUT;

        case ECONNRESET:
        case ECONNABORTED:
        case ENOTCONN:
        case EPIPE:
        case ENETDOWN:
        case ENETUNREACH:
        case ENETRESET:
        case ECONNREFUSED:
        case EHOSTDOWN:
        case EHOSTUNREACH:
            return ADB_ERR_DISCONNECTED;

        case EBADF:
        case EFAULT:
        case EINVAL:
            return ADB_ERR_IO;

        default:
            return ADB_ERR_NETWORK;
    }
}

adb_error_t adb__error_from_libusb(
        const int error)
{
    switch(error)
    {
        case LIBUSB_SUCCESS:
            return ADB_ERR_OK;

        case LIBUSB_ERROR_TIMEOUT:
            return ADB_ERR_TIMEOUT;

        case LIBUSB_ERROR_NO_DEVICE:
            return ADB_ERR_DISCONNECTED;

        case LIBUSB_ERROR_PIPE:
        case LIBUSB_ERROR_OVERFLOW:
        case LIBUSB_ERROR_IO:
            return ADB_ERR_USB;

        default:
            return ADB_ERR_USB;
    }
}
