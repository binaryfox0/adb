#ifndef ADBC_LOG_H
#define ADBC_LOG_H

#include <adb/adb_log.h>
#include <aparse.h>

#if defined(__clang__) || defined(__GNUC__)
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

#define ADBC_DEBUG(...) aparse_prog_debug(__VA_ARGS__)
#define ADBC_INFO(...) aparse_prog_info(__VA_ARGS__)
#define ADBC_WARN(...) aparse_prog_warn(__VA_ARGS__)
#define ADBC_ERROR(...) aparse_prog_error(__VA_ARGS__)

typedef struct yyjson_val yyjson_val;

ADB__PRINTF(1, 2) void adbc_log_err_errno(ADB__PRINTF_FMT const char *fmt, ...);
void adbc_log_debug_json(yyjson_val *item, const char *fmt, ...);
void adbc__adb_log_callback(void *userdata, adb_log_level_t level,
        const char *msg);
bool adbc_log_level_parse(const char *level_str, adb_log_level_t *out_level);

#endif
