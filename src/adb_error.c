#include <adb/adb_error.h>

const char *adb_strerror(
        const adb_error_t err)
{
    static const char *error_msgs[ADB__ERR_COUNT] =
    {
        [ADB_ERR_OK] = "no error",
        [ADB_ERR_GENERIC] = "generic error",
        [ADB_ERR_PARAM] = "paramaters error",
        [ADB_ERR_NO_MEM] = "out of memory",
        [ADB_ERR_USB] = "usb error",
        [ADB_ERR_NETWORK] = "network error"
    };
    if(err < 0 || err >= ADB__ERR_COUNT)
        return "unknown error";

    return error_msgs[err] ? 
            error_msgs[err] : "unassigned error";
}