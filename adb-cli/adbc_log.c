#include "adbc_log.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#include <yyjson.h>
#include "adbc_utils.h"
#include "adbc_io.h"

#define ADBC_LOG_BUFFER_SIZE 1024U

static const char *const adbc__log_level_strs[ADB__LOG_COUNT] =
{
    [ADB_LOG_DEBUG] = "debug",
    [ADB_LOG_INFO]  = "info",
    [ADB_LOG_WARN]  = "warn",
    [ADB_LOG_ERROR] = "error"
};

void adbc_log_err_errno(const char *fmt, ...)
{
    int saved_errno = errno;
    char buffer[ADBC_LOG_BUFFER_SIZE] = {0};
    va_list va;

    if(fmt == NULL)
        return;

    va_start(va, fmt);
    (void)vsnprintf(buffer, sizeof(buffer), fmt, va);
    va_end(va);

    ADBC_ERROR("%s", buffer);
    ADBC_INFO("reason: %s", strerror(saved_errno));
}

static void adbc_log_debug_json_impl(yyjson_val *item, const int depth)
{
    size_t idx = 0U;
    size_t max = 0U;
    yyjson_val *key = NULL;
    yyjson_val *value = NULL;

    if(item == NULL || depth < 0 || depth > 64)
    {
        ADBC_DEBUG("%*snull", depth >= 0 ? depth * 4 : 0, "");
        return;
    }

    switch(yyjson_get_type(item))
    {
        case YYJSON_TYPE_OBJ:
            ADBC_DEBUG("%*s{", depth * 4, "");
            yyjson_obj_foreach(item, idx, max, key, value)
            {
                ADBC_DEBUG("%*s\"%s\": ", (depth + 1) * 4, "",
                        yyjson_get_str(key) != NULL ? yyjson_get_str(key) : "");
                adbc_log_debug_json_impl(value, depth + 1);
            }
            ADBC_DEBUG("%*s}", depth * 4, "");
            break;

        case YYJSON_TYPE_ARR:
            ADBC_DEBUG("%*s[", depth * 4, "");
            yyjson_arr_foreach(item, idx, max, value)
                adbc_log_debug_json_impl(value, depth + 1);
            ADBC_DEBUG("%*s]", depth * 4, "");
            break;

        case YYJSON_TYPE_STR:
            ADBC_DEBUG("\"%s\"", yyjson_get_str(item) != NULL ? yyjson_get_str(item) : "");
            break;

        case YYJSON_TYPE_NUM:
            ADBC_DEBUG("%.17g", yyjson_get_num(item));
            break;

        case YYJSON_TYPE_BOOL:
            ADBC_DEBUG("%s", yyjson_get_bool(item) ? "true" : "false");
            break;

        case YYJSON_TYPE_NULL:
        default:
            ADBC_DEBUG("null");
            break;
    }
}

void adbc_log_debug_json(yyjson_val *item, const char *fmt, ...)
{
    char buffer[ADBC_LOG_BUFFER_SIZE] = {0};
    va_list va;

    if(fmt == NULL)
        return;

    va_start(va, fmt);
    (void)vsnprintf(buffer, sizeof(buffer), fmt, va);
    va_end(va);

    ADBC_DEBUG("%s", buffer);
    adbc_log_debug_json_impl(item, 0);
}

bool adbc_log_level_parse(const char *level_str, adb_log_level_t *out_level)
{
    if(out_level != NULL)
        *out_level = ADB_LOG_ERROR;

    if(level_str == NULL || out_level == NULL)
        return false;

    for(size_t i = 0U; i < ADBC_ARRSZ(adbc__log_level_strs); i++)
    {
        if(adbc__log_level_strs[i] != NULL &&
                strcmp(adbc__log_level_strs[i], level_str) == 0)
        {
            *out_level = (adb_log_level_t)i;
            return true;
        }
    }

    return false;
}

void adbc__adb_log_callback(
        void *userdata,
        adb_log_level_t level,
        const char *msg)
{
    intptr_t log_fd = (intptr_t)userdata;
    const char *message = msg != NULL ? msg : "";
    yyjson_mut_doc *doc = NULL;
    yyjson_mut_val *root = NULL;

    if(log_fd < 0)
    {
        switch(level)
        {
            case ADB_LOG_DEBUG: ADBC_DEBUG("%s", message); break;
            case ADB_LOG_INFO:  ADBC_INFO("%s", message); break;
            case ADB_LOG_WARN:  ADBC_WARN("%s", message); break;
            case ADB_LOG_ERROR: ADBC_ERROR("%s", message); break;
            case ADB__LOG_COUNT:
            default:
                ADBC_WARN("unknown ADB log level %d: %s", (int)level, message);
                break;
        }
        return;
    }

    if(log_fd > INT_MAX || (unsigned)level >= (unsigned)ADB__LOG_COUNT ||
            adbc__log_level_strs[(size_t)level] == NULL)
        return;

    doc = yyjson_mut_doc_new(NULL);
    if(doc == NULL)
        return;

    root = yyjson_mut_obj(doc);
    if(root == NULL)
        goto cleanup;

    yyjson_mut_doc_set_root(doc, root);
    if(!yyjson_mut_obj_add_str(doc, root, "type", "log") ||
            !yyjson_mut_obj_add_str(doc, root, "level",
                adbc__log_level_strs[(size_t)level]) ||
            !yyjson_mut_obj_add_str(doc, root, "msg", message))
        goto cleanup;

    (void)adbc_send_json((int)log_fd, doc);

cleanup:
    yyjson_mut_doc_free(doc);
}
