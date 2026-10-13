#include <adb/adb_error.h>

#include <errno.h>
#include <libusb.h>

const char *adb_strerror(
        const adb_error_t err)
{
    static const char *error_msgs[ADB__ERR_COUNT] =
    {
        [ADB_ERR_OK]            = "no error",
        [ADB_ERR_GENERIC]       = "unspecified error",
        [ADB_ERR_UNSUPPORTED]   = "operation not supported",
        [ADB_ERR_PARAM]         = "invalid parameter",
        [ADB_ERR_NO_MEM]        = "memory allocation failed",
        [ADB_ERR_IO]            = "I/O operation failed",
        [ADB_ERR_USB]           = "USB operation failed",
        [ADB_ERR_NETWORK]       = "network operation failed",
        [ADB_ERR_TIMEOUT]       = "operation timed out",
        [ADB_ERR_DISCONNECTED]  = "device disconnected",
        [ADB_ERR_PROTOCOL]      = "protocol violation error",
        [ADB_ERR_CRYPTO]        = "cryptographic operation failed",
        [ADB_ERR_TOO_SMALL]     = "given buffer  was too small",
        [ADB_ERR_TOO_LONG]      = "file path is too long",
        [ADB_ERR_COMPRESS]      = "compression/decompression error",
        [ADB_ERR_UNIMPLEMENTED] = "operation is not implemented",
        [ADB_ERR_AMBIGUOUS]     = "more than one device were found",
        [ADB_ERR_NOT_FOUND]     = "no device was found",
    };
    if(err < 0 || err >= ADB__ERR_COUNT)
        return "unknown error";

    return error_msgs[err] ? 
            error_msgs[err] : "unassigned error";
}

