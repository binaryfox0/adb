#ifndef ADB_ERROR_H
#define ADB_ERROR_H

typedef enum
{
    ADB_ERR_OK,
    ADB_ERR_GENERIC,
    ADB_ERR_PARAM,
    ADB_ERR_NO_MEM,
    ADB_ERR_USB,
    ADB_ERR_NETWORK,
    ADB__ERR_COUNT
} adb_error_t;

const char *adb_strerror(
        const adb_error_t err);

#endif