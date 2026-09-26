#ifndef ADB_COMPILER_H
#define ADB_COMPILER_H

#if defined(__clang__) || defined(__GNUC__)
#   define ADB__PRINTF(fmt_index, arg_index) \
        __attribute__((format(printf, fmt_index, arg_index)))
#   define ADB__PRINTF_FMT
#   define ADB__NODISCARD __attribute__((warn_unused_result))
#elif defined(_MSC_VER)
#   include <sal.h>
#   define ADB__PRINTF(fmt_index, arg_index)
#   define ADB__PRINTF_FMT _Printf_format_string_
#   define ADB__NODISCARD _Check_return_
#else
#   define ADB__NODISCARD
#   define ADB__PRINTF(fmt_index, arg_index)
#   define ADB__PRINTF_FMT
#endif

#endif
