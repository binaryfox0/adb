#include <adb/adb_log.h>
#include "adb_log_priv.h"

#include <stdio.h>
#include <stdarg.h>

#include <mbedtls/error.h>

static adb_log_callback_t adb__log_callback = NULL;
static void *adb__log_userdata = NULL;
static adb_log_level_t adb__log_level = ADB_LOG_ERROR;

adb_error_t adb_set_log_callback(
        const adb_log_callback_t callback,
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

void adb__log(
        const adb_log_level_t level,
        const char *fmt,
        ...)
{
    char buffer[4096] = {0};
    va_list va;

    if(!adb__log_callback)
        return;
    if(level < adb__log_level)
        return;

    va_start(va, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, va);
    va_end(va);

    adb__log_callback(
            adb__log_userdata, level, 
            buffer);
}

void adb__log_err_mbedtls(
        const char *label,
        const int err)
{
    char buf[256] = {0};
    mbedtls_strerror(err, buf, sizeof(buf));
    ADB__ERROR("%s", label);
    ADB__INFO("reason: %s", buf);
}
