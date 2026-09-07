#include "adb/adb_log.h"
#include <aparse.h>
#include <adb/adb.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define error aparse_prog_error
#define info aparse_prog_info

#define CHECK(x, msg, res, label) \
    if(((res) = x) != ADB_ERR_OK) \
    { \
        error((msg)); \
        info("reason: %s", adb_strerror((res))); \
        goto label; \
    }

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

static void query_command(
        const aparse_arg *args,
        void *param)
{
    adb_error_t err = ADB_ERR_OK;
    adb_ctx_t *ctx = NULL;
    adb_wired_info_t **infos = NULL;
    size_t count = 0;

    (void)args;
    (void)param;
    
    CHECK(adb_ctx_create(&ctx), "failed to create libadb context", err, cleanup);
    CHECK(adb_query_wired(ctx, &infos, &count), 
            "failed to query wired devices", err, cleanup);
    for(size_t i = 0; i < count; i++)
    {
        adb_wired_info_t *conn_info = infos[i];
        info("Device %zu: %s - %s", i,
                adb_wired_info_manufacturer(conn_info),
                adb_wired_info_product(conn_info));
    }
cleanup:
    adb_ctx_destroy(ctx);
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
    adb_ctx_t *ctx = NULL;
    adb_conn_t *conn = NULL;
    adb_key_t *key = NULL;

    (void)args;

    host = ip;
    colon = (char*)(uintptr_t)strchr(ip, ':');

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
        error("Invalid address \"%s\": port must be between 1 and %d",
              ip, UINT16_MAX);
        return;
    }

    CHECK(adb_ctx_create(&ctx), 
            "failed to create libadb context", err, cleanup);
    CHECK(adb_conn_create_wireless(&conn, ctx, 
                host, (uint16_t)port), 
            "failed to create connection to wireless device", err, cleanup);
   
    CHECK(adb_key_generate(&key, ctx),
            "failed to generate new key", err, cleanup);
    CHECK(adb_conn_pair(conn, code, 6, key), 
            "failed to pair with given device", err, cleanup);

cleanup:
    adb_key_destroy(key);
    adb_conn_destroy(conn);
    adb_ctx_destroy(ctx);
}

static void pubkey_command(
        const aparse_arg *args, 
        void *param)
{
    const char *path = *(const char**)param;
    const char *output = ((const char**)param)[1];

    adb_error_t err = ADB_ERR_OK;
    adb_ctx_t *ctx = NULL;
    adb_key_t *key = NULL;
    uint8_t pubkey[2048] = {0};
    size_t pubkey_size = 0;

    (void)args;

    CHECK(adb_ctx_create(&ctx),
            "failed to create libadb context", err, cleanup);
    CHECK(adb_key_load(&key, ctx, path),
            "failed to load adb private key", err, cleanup);
    CHECK(adb_key_generate_pubkey(
                key, pubkey, sizeof(pubkey), &pubkey_size),
            "failed to generate public key from private key", err, cleanup);

    if(output)
    {
        FILE *file = NULL;
        size_t pubkey_len = 0;

        file = fopen(output, "w");
        if(!file)
        {
            error("failed to open output file");
            info("reason: %s", strerror(errno));
            goto cleanup;
        }

        pubkey_len = strlen((char*)pubkey);
        if(fwrite(pubkey, 1, pubkey_len, file) != pubkey_len)
        {
            error("failed to write to output file");
            info("reason: %s", strerror(errno));
            goto cleanup;
        }

        fclose(file);
    } else
        printf("%s\n", pubkey);

cleanup:
    adb_key_destroy(key);
    adb_ctx_destroy(ctx);
}

int main(int argc, char **argv)
{
    aparse_arg pair_args[] =
    {
        aparse_arg_string(
                "ip", 
                NULL, 0, 
                "IP to target device (host:port)"),
        aparse_arg_string(
                "code",
                NULL, 0, 
                "Pairing code alongside with the IP"),
        aparse_arg_end_marker
    };

    aparse_arg pubkey_args[] = 
    {
        aparse_arg_string("path", 
                NULL, 0, 
                "Path to RSA-2048 private key"),
        aparse_arg_option(
                "-o", "--output", 
                NULL, 0,
                APARSE_ARG_TYPE_STRING,
                "Path to output file"),
        aparse_arg_end_marker
    };
    aparse_arg commands[] =
    {
        aparse_arg_subparser_impl(
                "pair", 
                pair_args, pair_command, 
                NULL, 0, 
                "Pair wireless ADB device through TCP",
                (size_t[]){
                    0, sizeof(const char*),
                    sizeof(const char*), sizeof(const char*)
                }, 2),
        aparse_arg_subparser(
                "query",
                NULL, query_command,
                NULL, 0,
                "Query all USB connected ADB devices"),
        aparse_arg_subparser_impl(
                "pubkey", 
                pubkey_args, pubkey_command, 
                NULL, 0, 
                "Generate public key from private key", 
                (size_t[]){
                    0, sizeof(void*),
                    sizeof(void*), sizeof(void*)
                }, 2),
        aparse_arg_end_marker
    };
    aparse_arg main_args[] =
    {
        aparse_arg_parser("command", commands),
        aparse_arg_end_marker
    };
    aparse_list dispatch = {0};

    if(aparse_parse(
            argc, argv,
            main_args, &dispatch,
            "adb cli interface") != APARSE_STATUS_OK)
        return 1;

    adb_set_log_callback(log_callback, NULL, 
            ADB_LOG_DEBUG);
    aparse_dispatch_all(&dispatch);
    return 0;
}
