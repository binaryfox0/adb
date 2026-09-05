#include <adb/adb_log.h>
#include "adb_log_priv.h"

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>

#include <mbedtls/error.h>

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
