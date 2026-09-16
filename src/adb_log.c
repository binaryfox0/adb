#include <adb/adb_log.h>
#include "adb_log_priv.h"

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>

#include <mbedtls/error.h>

#include "adb_utils.h"

#define ADB__LOG_BUFFER_SIZE 4096

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

static void adb__log_variadic(
        const adb_log_level_t level,
        const char *fmt,
        va_list va)
{
    char buffer[ADB__LOG_BUFFER_SIZE] = {0};
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
    adb__log_variadic(level, fmt, va);
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
    adb__log_variadic(ADB_LOG_ERROR, fmt, va);
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
    adb__log_variadic(ADB_LOG_ERROR, fmt, va);
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
    adb__log_variadic(ADB_LOG_ERROR, fmt, va);
    va_end(va);
    ADB__INFO("reason: %s", adb_strerror(err));
}

#define ADB__BYTES_PER_LINE 16

static inline void adb__log_flush(
        const char *buffer, 
        size_t *idx)
{
    if(!buffer || !idx)
        return;

    adb__log(ADB_LOG_DEBUG, "%.*s", (int)*idx, buffer);
    *idx = 0;
}

void adb__log_multiline_text(
        const char *text,
        const char *fmt,
        ...)
{
    va_list va;
    char buffer[ADB__LOG_BUFFER_SIZE];
    size_t idx = 0;

    if(ADB_LOG_DEBUG < adb__log_level)
        return;

    va_start(va, fmt);
    adb__log_variadic(ADB_LOG_DEBUG, fmt, va);
    va_end(va);

    for(; *text != '\0';)
    {
        const char *escaped = NULL;
        size_t len = 1;

        switch(*text)
        {
            case '\t':
                escaped = "\\t";
                len = 2;
                break;

            case '\r':
                escaped = (text[1] == '\n') ? "\\r\\n" : "\\r";
                len = strlen(escaped);
                break;

            case '\n':
                escaped = "\\n";
                len = 2;
                break;

            default:
                if(idx == sizeof(buffer))
                    adb__log_flush(buffer, &idx);

                buffer[idx++] = *text++;
                continue;
        }

        if(idx + len > sizeof(buffer))
            adb__log_flush(buffer, &idx);

        memcpy(buffer + idx, escaped, len);
        idx += len;

        adb__log_flush(buffer, &idx);
        text += (text[0] == '\r' && text[1] == '\n') ? 2 : 1;
    }

    adb__log_flush(buffer, &idx);
}

void adb__log_payload(
        const void *data,
        const size_t size,
        const char *fmt,
        ...)
{
    va_list va;
    const uint8_t *bytes = NULL;
    size_t line_count = 0;

    if(ADB_LOG_DEBUG < adb__log_level)
        return;

    bytes = (const uint8_t*)data;
    line_count = (size + (ADB__BYTES_PER_LINE - 1)) / ADB__BYTES_PER_LINE;

    va_start(va, fmt);
    adb__log_variadic(ADB_LOG_DEBUG, fmt, va);
    va_end(va);

    for(size_t i = 0; i < line_count; i++)
    {
        size_t offset = i * ADB__BYTES_PER_LINE;
        size_t line_size = ADB__MIN(size - offset, ADB__BYTES_PER_LINE);
        /*
         * 8  = offset
         * 2  = spaces after offset
         * 3  = "XX " for each byte
         * 16 = ASCII representation
         * 1  = NUL terminator
         */
        char buffer[8 + 2 + (ADB__BYTES_PER_LINE * 3)
                       + ADB__BYTES_PER_LINE + 1] = {0};
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
