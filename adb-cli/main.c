#include "adb/adb_log.h"
#include <aparse.h>
#include <adb/adb.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define error aparse_prog_error
#define info aparse_prog_info

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

static void pair_command(
        const aparse_arg *args,
        void *param)
{
    const char *ip = ((const char**)param)[0];
    const char *code = ((const char**)param)[1];

    const char *host = NULL;
    char *colon = NULL;
    long port = 0;
    char *end = NULL;

    adb_error_t err = ADB_ERR_OK;
    adb_conn_t *conn = NULL;

    (void)args;

    host = ip;
    colon = strchr(ip, ':');

    if (!colon)
    {
        error("Invalid address \"%s\": missing port (expected host:port)", ip);
        return;
    }

    *colon = '\0';

    errno = 0;
    port = strtol(colon + 1, &end, 10);
    if(
            errno == ERANGE || 
            port == LONG_MIN || port == LONG_MAX)
    {
        error("Invalid address \"%s\": port is out of range", ip);
        return;
    }

    if(end == colon + 1 || *end != '\0')
    {
        error("Invalid address \"%s\": port must be a number", ip);
        return;
    }

    if(port == 0 || port > UINT16_MAX)
    {
        error("Invalid address \"%s\": port must be between 1 and %u",
              ip, UINT16_MAX);
        return;
    }

    CHECK(adb_conn_create_wireless(&conn, 
                host, (uint16_t)port), err, cleanup);

cleanup:
    adb_conn_destroy(conn);
}

int main(int argc, char **argv)
{
    aparse_arg pair_args[] =
    {
        aparse_arg_end_marker
    };
    aparse_arg commands[] =
    {
        aparse_arg_subparser(
                "pair", 
                pair_args, pair_command, 
                NULL, 0, 
                "Pair wireless ADB device through TCP"),
        aparse_arg_end_marker
    };
    aparse_arg main_args[] =
    {
        aparse_arg_parser("command", commands),
        aparse_arg_end_marker
    };
    aparse_list dispatch = {0};

    adb_ctx_t *ctx = NULL;
    adb_usb_info_t **infos = NULL;
    size_t count = 0;

    if(aparse_parse(
            argc, argv,
            main_args, &dispatch,
            "adb cli interface") != APARSE_STATUS_OK)
        return 1;

    adb_set_log_callback(log_callback, NULL, 
            ADB_LOG_DEBUG);
    aparse_dispatch_all(&dispatch);
    adb_ctx_create(&ctx);
    adb_query_usb(ctx, &infos, &count);
    for(size_t i = 0; i < count; i++)
    {
        adb_usb_info_t *conn_info = infos[i];
        info("Device %zu: %s - %s", i,
                adb_conn_info_get_manufacturer(conn_info),
                adb_conn_info_get_product(conn_info));
    }
    adb_ctx_destroy(ctx);
    return 0;
}
