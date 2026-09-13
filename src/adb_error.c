#include <adb/adb_error.h>
#include "adb_error_priv.h"

#include <errno.h>
#include <libusb.h>

const char *adb_strerror(
        const adb_error_t err)
{
    static const char *error_msgs[ADB__ERR_COUNT] =
    {
        [ADB_ERR_OK]           = "no error",
        [ADB_ERR_GENERIC]      = "unspecified error",
        [ADB_ERR_UNSUPPORTED]  = "operation not supported",
        [ADB_ERR_PARAM]        = "invalid parameter",
        [ADB_ERR_NO_MEM]       = "memory allocation failed",
        [ADB_ERR_IO]           = "I/O operation failed",
        [ADB_ERR_USB]          = "USB operation failed",
        [ADB_ERR_NETWORK]      = "network operation failed",
        [ADB_ERR_TIMEOUT]      = "operation timed out",
        [ADB_ERR_DISCONNECTED] = "device disconnected",
        [ADB_ERR_PROTOCOL]     = "protocol error",
        [ADB_ERR_CRYPTO]       = "cryptographic operation failed",
        [ADB_ERR_TOO_SMALL]        = "buffer too small",
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
            return ADB_ERR_TIMEOUT;
// 
//         case EAGAIN:
// #if  EWOULDBLOCK != EAGAIN
//         case EWOULDBLOCK:
// #endif
//             return ADB_ERR_WOULDBLOCK;

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
