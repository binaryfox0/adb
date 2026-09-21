#ifndef ADB_ERROR_H
#define ADB_ERROR_H

#define ADB_FUNC_ERR(err) ((int)-(err))

typedef enum
{
    ADB_ERR_OK,
    ADB_ERR_GENERIC,
    ADB_ERR_PARAM,
    ADB_ERR_UNSUPPORTED,
    ADB_ERR_NO_MEM,
    ADB_ERR_IO,
    ADB_ERR_USB,
    ADB_ERR_NETWORK,
    ADB_ERR_TIMEOUT,
    ADB_ERR_DISCONNECTED,
    ADB_ERR_PROTOCOL,
    ADB_ERR_CRYPTO,
    ADB_ERR_TOO_SMALL,
    ADB_ERR_COMPRESS,
    ADB__ERR_COUNT
} adb_error_t;

const char *adb_strerror(
        const adb_error_t err);

#endif
