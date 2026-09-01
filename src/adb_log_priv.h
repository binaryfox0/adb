#ifndef ADB_LOG_PRIV_H
#define ADB_LOG_PRIV_H

#include <adb/adb_log.h>

#if defined(__GNUC__) || defined(__clang__)
#   define ADB__PRINTF(fmt_index, arg_index) \
        __attribute__((format(printf, fmt_index, arg_index)))
#   define ADB__PRINTF_FMT
#elif defined(_MSC_VER)
#   define ADB__PRINTF(fmt_index, arg_index)
#   define ADB__PRINTF_FMT _Printf_format_string_
#else
#   define ADB__PRINTF(fmt_index, arg_index)
#   define ADB__PRINTF_FMT
#endif

#define ADB__DEBUG(...) adb__log(ADB_LOG_DEBUG, __VA_ARGS__)
#define ADB__INFO(...)  adb__log(ADB_LOG_INFO,  __VA_ARGS__)
#define ADB__WARN(...)  adb__log(ADB_LOG_WARN, __VA_ARGS__)
#define ADB__ERROR(...) adb__log(ADB_LOG_ERROR, __VA_ARGS__)

ADB__PRINTF(2, 3) void adb__log(
        const adb_log_level_t level,
        ADB__PRINTF_FMT const char *fmt,
        ...);

void adb__log_err_mbedtls(
        const char *label,
        const int err);

#endif
