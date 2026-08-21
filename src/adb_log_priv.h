#ifndef ADB_LOG_PRIV_H
#define ADB_LOG_PRIV_H

#include <adb/adb_log.h>

#define ADB__DEBUG(...) adb__log(ADB_LOG_DEBUG, __VA_ARGS__)
#define ADB__INFO(...)  adb__log(ADB_LOG_INFO,  __VA_ARGS__)
#define ADB__WARN(...)  adb__log(ADB_LOG_WARN, __VA_ARGS__)
#define ADB__ERROR(...) adb__log(ADB_LOG_ERROR, __VA_ARGS__)

void adb__log(
        const adb_log_level_t level,
        const char *fmt,
        ...);

#endif