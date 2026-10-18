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

static void adbc__log_debug_json_immut_impl(
        yyjson_val *item,
        const int depth,
        const char *key,
        const int has_next)
{
    switch (yyjson_get_type(item))
    {
        case YYJSON_TYPE_OBJ:
        {
            const size_t count = yyjson_obj_size(item);
            yyjson_obj_iter iter = yyjson_obj_iter_with(item);
            yyjson_val *obj_key;

            ADBC_DEBUG(
                    "%*s"
                    "%s%s%s"
                    "{%s",
                    depth * 4, "",
                    key ? "\"" : "",
                    key ? key : "",
                    key ? "\": " : "",
                    count ? "" : "}"
            );

            size_t i = 0;
            while ((obj_key = yyjson_obj_iter_next(&iter)))
            {
                yyjson_val *child = yyjson_obj_iter_get_val(obj_key);

                adbc__log_debug_json_immut_impl(
                        child,
                        depth + 1,
                        yyjson_get_str(obj_key),
                        ++i < count
                );
            }

            if (count)
            {
                ADBC_DEBUG("%*s}%s", depth * 4, "",
                        has_next ? "," : "");
            }
            break;
        }

        case YYJSON_TYPE_ARR:
        {
            const size_t count = yyjson_arr_size(item);
            yyjson_arr_iter iter = yyjson_arr_iter_with(item);
            yyjson_val *child;

            ADBC_DEBUG(
                    "%*s"
                    "%s%s%s"
                    "[%s",
                    depth * 4, "",
                    key ? "\"" : "",
                    key ? key : "",
                    key ? "\": " : "",
                    count ? "" : "]"
            );

            size_t i = 0;
            while ((child = yyjson_arr_iter_next(&iter)))
            {
                adbc__log_debug_json_immut_impl(
                        child,
                        depth + 1,
                        NULL,
                        ++i < count
                );
            }

            if (count)
            {
                ADBC_DEBUG("%*s]%s", depth * 4, "",
                        has_next ? "," : "");
            }
            break;
        }

        case YYJSON_TYPE_STR:
            ADBC_DEBUG(
                    "%*s"
                    "%s%s%s"
                    "\"%s\"%s",
                    depth * 4, "",
                    key ? "\"" : "",
                    key ? key : "",
                    key ? "\": " : "",
                    yyjson_get_str(item),
                    has_next ? "," : "");
            break;

        case YYJSON_TYPE_NUM:
            ADBC_DEBUG(
                    "%*s"
                    "%s%s%s"
                    "%.17g%s",
                    depth * 4, "",
                    key ? "\"" : "",
                    key ? key : "",
                    key ? "\": " : "",
                    yyjson_get_num(item),
                    has_next ? "," : "");
            break;

        case YYJSON_TYPE_BOOL:
            ADBC_DEBUG(
                    "%*s"
                    "%s%s%s"
                    "%s%s",
                    depth * 4, "",
                    key ? "\"" : "",
                    key ? key : "",
                    key ? "\": " : "",
                    yyjson_get_bool(item) ? "true" : "false",
                    has_next ? "," : "");
            break;

        case YYJSON_TYPE_NULL:
        default:
            ADBC_DEBUG(
                    "%*s"
                    "%s%s%s"
                    "null%s",
                    depth * 4, "",
                    key ? "\"" : "",
                    key ? key : "",
                    key ? "\": " : "",
                    has_next ? "," : "");
            break;
    }
}

static void adbc__log_debug_json_mut_impl(
        yyjson_mut_val *item,
        const int depth,
        const char *key,
        const int has_next)
{
    switch (yyjson_mut_get_type(item))
    {
        case YYJSON_TYPE_OBJ:
        {
            const size_t count = yyjson_mut_obj_size(item);
            yyjson_mut_obj_iter iter = yyjson_mut_obj_iter_with(item);
            yyjson_mut_val *obj_key;

            ADBC_DEBUG(
                    "%*s"
                    "%s%s%s"
                    "{%s",
                    depth * 4, "",
                    key ? "\"" : "",
                    key ? key : "",
                    key ? "\": " : "",
                    count ? "" : "}"
            );

            size_t i = 0;
            while ((obj_key = yyjson_mut_obj_iter_next(&iter)))
            {
                yyjson_mut_val *child = yyjson_mut_obj_iter_get_val(obj_key);
                adbc__log_debug_json_mut_impl(
                        child,
                        depth + 1,
                        yyjson_mut_get_str(obj_key),
                        ++i < count
                );
            }

            if (count)
            {
                ADBC_DEBUG("%*s}%s", depth * 4, "",
                        has_next ? "," : "");
            }
            break;
        }

        case YYJSON_TYPE_ARR:
        {
            const size_t count = yyjson_mut_arr_size(item);
            yyjson_mut_arr_iter iter = yyjson_mut_arr_iter_with(item);
            yyjson_mut_val *child;

            ADBC_DEBUG(
                    "%*s"
                    "%s%s%s"
                    "[%s",
                    depth * 4, "",
                    key ? "\"" : "",
                    key ? key : "",
                    key ? "\": " : "",
                    count ? "" : "]"
            );

            size_t i = 0;
            while ((child = yyjson_mut_arr_iter_next(&iter)))
            {
                adbc__log_debug_json_mut_impl(
                        child,
                        depth + 1,
                        NULL,
                        ++i < count
                );
            }

            if (count)
            {
                ADBC_DEBUG("%*s]%s", depth * 4, "",
                        has_next ? "," : "");
            }
            break;
        }

        case YYJSON_TYPE_STR:
            ADBC_DEBUG(
                    "%*s"
                    "%s%s%s"
                    "\"%s\"%s",
                    depth * 4, "",
                    key ? "\"" : "",
                    key ? key : "",
                    key ? "\": " : "",
                    yyjson_mut_get_str(item),
                    has_next ? "," : "");
            break;

        case YYJSON_TYPE_NUM:
            ADBC_DEBUG(
                    "%*s"
                    "%s%s%s"
                    "%.17g%s",
                    depth * 4, "",
                    key ? "\"" : "",
                    key ? key : "",
                    key ? "\": " : "",
                    yyjson_mut_get_num(item),
                    has_next ? "," : "");
            break;

        case YYJSON_TYPE_BOOL:
            ADBC_DEBUG(
                    "%*s"
                    "%s%s%s"
                    "%s%s",
                    depth * 4, "",
                    key ? "\"" : "",
                    key ? key : "",
                    key ? "\": " : "",
                    yyjson_mut_get_bool(item) ? "true" : "false",
                    has_next ? "," : "");
            break;

        case YYJSON_TYPE_NULL:
        default:
            ADBC_DEBUG(
                    "%*s"
                    "%s%s%s"
                    "null%s",
                    depth * 4, "",
                    key ? "\"" : "",
                    key ? key : "",
                    key ? "\": " : "",
                    has_next ? "," : "");
            break;
    }
}

void adbc_log_debug_json_immut(yyjson_val *item, const char *fmt, ...)
{
    char buffer[ADBC_LOG_BUFFER_SIZE] = {0};
    va_list va;

    if(fmt == NULL)
        return;

    va_start(va, fmt);
    (void)vsnprintf(buffer, sizeof(buffer), fmt, va);
    va_end(va);

    ADBC_DEBUG("%s", buffer);
    adbc__log_debug_json_immut_impl(item, 0, NULL, 0);
}

void adbc_log_debug_json_mut(yyjson_mut_val *item, const char *fmt, ...)
{
    char buffer[ADBC_LOG_BUFFER_SIZE] = {0};
    va_list va;

    if(fmt == NULL)
        return;

    va_start(va, fmt);
    (void)vsnprintf(buffer, sizeof(buffer), fmt, va);
    va_end(va);

    ADBC_DEBUG("%s", buffer);
    adbc__log_debug_json_mut_impl(item, 0, NULL, 0);
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

void adbc_log_server_callback(
        void *userdata,
        adb_log_level_t level,
        const char *msg)
{
    intptr_t log_fd = (intptr_t)userdata;
    const char *message = msg != NULL ? msg : "";
    yyjson_mut_doc *doc = NULL;
    yyjson_mut_val *root = NULL;

    switch(level)
    {
        case ADB_LOG_DEBUG: 
            aparse_log("adb", APARSE__DEBUG_LABEL, "%s", message); 
            break;
        case ADB_LOG_INFO:
            aparse_log("adb", APARSE__INFO_LABEL, "%s", message); 
            break;
        case ADB_LOG_WARN:
            aparse_log("adb", APARSE__WARN_LABEL, "%s", message); 
            break;
        case ADB_LOG_ERROR:
            aparse_log("adb", APARSE__ERROR_LABEL, "%s", message); 
            break;

        case ADB__LOG_COUNT:
        default: break;
    }
    if(log_fd < 0)
        return;

    doc = yyjson_mut_doc_new(NULL);
    if(!doc)
        return;

    root = yyjson_mut_obj(doc);
    if(!root)
        goto cleanup;

    yyjson_mut_doc_set_root(doc, root);
    if(
            !yyjson_mut_obj_add_str(doc, root, "type", "log") ||
            !yyjson_mut_obj_add_str(doc, root, "level",
                adbc__log_level_strs[(size_t)level]) ||
            !yyjson_mut_obj_add_str(doc, root, "msg", message))
        goto cleanup;

    (void)adbc_send_json((int)log_fd, doc);

cleanup:
    yyjson_mut_doc_free(doc);
}

bool adbc_log_handle_server(
        yyjson_val *root)
{
    yyjson_val *level = yyjson_obj_get(root, "level");
    yyjson_val *msg = yyjson_obj_get(root, "msg");
    const char *level_str = NULL;
    const char *message = NULL;

    if(!yyjson_is_str(level) || !yyjson_is_str(msg))
    {
        ADBC_ERROR("malformed server log packet");
        return false;
    }

    level_str = yyjson_get_str(level);
    message = yyjson_get_str(msg);
    if(strcmp(level_str, "debug") == 0)
        aparse_log("adb", APARSE__DEBUG_LABEL, "%s", message); 
    else if(strcmp(level_str, "info") == 0)
        aparse_log("adb", APARSE__INFO_LABEL, "%s", message); 
    else if(strcmp(level_str, "warn") == 0)
        aparse_log("adb", APARSE__WARN_LABEL, "%s", message); 
    else if(strcmp(level_str, "error") == 0)
        aparse_log("adb", APARSE__ERROR_LABEL, "%s", message); 
    else
        aparse_log("adb", "unknown", "%s", message); 

    return true;
}

void adbc_log_client_callback(
        void *userdata,
        adb_log_level_t level,
        const char *msg)
{
    (void)userdata;
    switch(level)
    {
        case ADB_LOG_DEBUG: 
            aparse_log("adb", APARSE__DEBUG_LABEL, "%s", msg);
            break;

        case ADB_LOG_INFO: 
            aparse_log("adb", APARSE__INFO_LABEL, "%s", msg);
            break;

        case ADB_LOG_WARN: 
            aparse_log("adb", APARSE__WARN_LABEL, "%s", msg);
            break;
        case ADB_LOG_ERROR:
            aparse_log("adb", APARSE__ERROR_LABEL, "%s", msg);
            break;

        case ADB__LOG_COUNT:
        default: break;
    }
}

