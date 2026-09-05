#ifndef ADB_LOG_H
#define ADB_LOG_H

#include <adb/adb_error.h>

typedef enum
{
    ADB_LOG_DEBUG,
    ADB_LOG_INFO,
    ADB_LOG_WARN,
    ADB_LOG_ERROR,
    ADB__LOG_COUNT
} adb_log_level_t;

typedef void (*adb_log_fn)(
        void *userdata,
        adb_log_level_t level,
        const char *msg);

adb_error_t adb_set_log_callback(
        const adb_log_fn callback,
        void *userdata,
        const adb_log_level_t level);

#endif
