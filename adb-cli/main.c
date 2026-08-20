#include "adb/adb_log.h"
#include <aparse.h>
#include <adb/adb.h>

#define CHECK(x, res, label) \
    if(((res) = x) != ADB_ERR_OK) goto label

static void log_callback(
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
        default:
            break;
    }
}

int main(int argc, char **argv)
{
    aparse_arg main_args[] =
    {
        aparse_arg_end_marker
    };

    adb_ctx_t *ctx = NULL;
    adb_conn_info_t **infos = NULL;
    size_t count = 0;

    if(aparse_parse(
            argc, argv,
            main_args, NULL,
            "adb cli interface") != APARSE_STATUS_OK)
        return 1;

    adb_set_log_callback(log_callback, NULL, ADB_LOG_DEBUG);
    adb_ctx_create(&ctx);
    adb_conn_query(ctx, &infos, &count);
    adb_ctx_destroy(ctx);
    return 0;
}