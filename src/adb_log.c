#include <adb/adb_log.h>
#include "adb_log_priv.h"

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>

#include <mbedtls/error.h>

#include "adb_utils.h"

static adb_log_fn adb__log_callback = NULL;
static void *adb__log_userdata = NULL;
static adb_log_level_t adb__log_level = ADB_LOG_ERROR;

adb_error_t adb_set_log_callback(
        const adb_log_fn callback,
        void *userdata,
        const adb_log_level_t level)
{
    if(level < 0 || level >= ADB__LOG_COUNT)
        return ADB_ERR_PARAM;

    if(!callback)
    {
        adb__log_callback = NULL;
        adb__log_userdata = NULL;
        adb__log_level = ADB_LOG_ERROR;
        return ADB_ERR_OK;
    }

    adb__log_callback = callback;
    adb__log_userdata = userdata;
    adb__log_level = level;
    return ADB_ERR_OK;
}

static void adb__vlog(
        const adb_log_level_t level,
        const char *fmt,
        va_list va)
{
    char buffer[4096] = {0};
    if(!adb__log_callback)
        return;
    if(level < adb__log_level)
        return;

    vsnprintf(buffer, sizeof(buffer), fmt, va);
    adb__log_callback(
            adb__log_userdata, level, 
            buffer);
}

void adb__log(
        const adb_log_level_t level,
        const char *fmt,
        ...)
{
    va_list va;
    va_start(va, fmt);
    adb__vlog(level, fmt, va);
    va_end(va);
}

void adb__log_err_mbedtls(
        const int err,
        const char *fmt,
        ...)
{
    va_list va;
    char buf[256] = {0};

    va_start(va, fmt);
    adb__vlog(ADB_LOG_ERROR, fmt, va);
    va_end(va);
    mbedtls_strerror(err, buf, sizeof(buf));
    ADB__INFO("reason: %s", buf);
}

void adb__log_err_errno(
        const char *fmt,
        ...)
{
    va_list va;

    va_start(va, fmt);
    adb__vlog(ADB_LOG_ERROR, fmt, va);
    va_end(va);
    ADB__INFO("reason: %s", strerror(errno));
}

void adb__log_err_adb(
        const adb_error_t err,
        const char *fmt,
        ...)
{
    va_list va;

    va_start(va, fmt);
    adb__vlog(ADB_LOG_ERROR, fmt, va);
    va_end(va);
    ADB__INFO("reason: %s", adb_strerror(err));
}

#define ADB__BYTES_PER_LINE 16

void adb__log_print_payload(
        const void *data,
        const size_t size,
        const char *fmt,
        ...)
{
    va_list va;
    const unsigned char *bytes = (const unsigned char *)data;

    if(ADB_LOG_DEBUG < adb__log_level)
        return;

    va_start(va, fmt);
    adb__vlog(ADB_LOG_DEBUG, fmt, va);
    va_end(va);

    const size_t line_count =
        (size + (ADB__BYTES_PER_LINE - 1)) / ADB__BYTES_PER_LINE;

    for(size_t i = 0; i < line_count; i++)
    {
        const size_t offset = i * ADB__BYTES_PER_LINE;
        const size_t line_size =
            (size - offset) < ADB__BYTES_PER_LINE
                ? (size - offset)
                : ADB__BYTES_PER_LINE;

        /*
         * 8  = offset
         * 2  = spaces after offset
         * 3  = "XX " for each byte
         * 16 = ASCII representation
         * 1  = NUL terminator
         */
        char buffer[8 + 2 + (ADB__BYTES_PER_LINE * 3)
                       + ADB__BYTES_PER_LINE + 1];

        size_t pos = 0;

        /* Offset */
        pos += (size_t)snprintf(
            buffer + pos,
            sizeof(buffer) - pos,
            "%08zX  ",
            offset);

        /* Hex */
        for(size_t j = 0; j < ADB__BYTES_PER_LINE; j++)
        {
            if(j < line_size)
            {
                pos += (size_t)snprintf(
                    buffer + pos,
                    sizeof(buffer) - pos,
                    "%02X ",
                    bytes[offset + j]);
            }
            else
            {
                pos += (size_t)snprintf(
                    buffer + pos,
                    sizeof(buffer) - pos,
                    "   ");
            }
        }

        /* ASCII */
        for(size_t j = 0; j < ADB__BYTES_PER_LINE; j++)
        {
            if(j < line_size)
            {
                const unsigned char c = bytes[offset + j];

                buffer[pos++] =
                    (c >= 0x20 && c <= 0x7E)
                        ? (char)c
                        : '.';
            }
            else
            {
                buffer[pos++] = ' ';
            }
        }

        buffer[pos] = '\0';

        adb__log_callback(
                adb__log_userdata, 
                ADB_LOG_DEBUG, 
                buffer);
    }
}
