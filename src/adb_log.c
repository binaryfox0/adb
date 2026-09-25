#include <adb/adb_log.h>
#include "adb_log_priv.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
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

#define ADB__HEX_BYTES_PER_LINE 16


void adb__log_multiline_text(
        const char *text,
        const char *fmt,
        ...)
{
    va_list va;
    if (ADB_LOG_DEBUG < adb__log_level)
        return;

    va_start(va, fmt);
    adb__log_variadic(ADB_LOG_DEBUG, fmt, va);
    va_end(va);

    const char *line_start = text;

    while (*text != '\0')
    {
        if (*text == '\r' || *text == '\n')
        {
            ADB__DEBUG("%.*s", (int)(text - line_start), line_start);
            if (*text == '\r' && text[1] == '\n')
                text += 2;
            else
                text++;

            line_start = text;
        }
        else
            text++;
    }

    if (text != line_start)
        ADB__DEBUG("%.*s", (int)(text - line_start), line_start);
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
    line_count = (size + (ADB__HEX_BYTES_PER_LINE - 1)) / ADB__HEX_BYTES_PER_LINE;

    va_start(va, fmt);
    adb__log_variadic(ADB_LOG_DEBUG, fmt, va);
    va_end(va);

    for(size_t i = 0; i < line_count; i++)
    {
        size_t offset = i * ADB__HEX_BYTES_PER_LINE;
        size_t line_size = ADB__MIN(size - offset, ADB__HEX_BYTES_PER_LINE);
        /*
         * 8  = offset
         * 2  = spaces after offset
         * 3  = "XX " for each byte
         * 16 = ASCII representation
         * 1  = NUL terminator
         */
        char buffer[8 + 2 + (ADB__HEX_BYTES_PER_LINE * 3)
                       + ADB__HEX_BYTES_PER_LINE + 1] = {0};
        size_t pos = 0;

        /* Offset */
        pos += (size_t)snprintf(
            buffer + pos,
            sizeof(buffer) - pos,
            "%08zX  ",
            offset);

        /* Hex */
        for(size_t j = 0; j < ADB__HEX_BYTES_PER_LINE; j++)
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
        for(size_t j = 0; j < ADB__HEX_BYTES_PER_LINE; j++)
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
