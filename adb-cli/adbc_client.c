#include "adbc_client.h"

#include <string.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <yyjson.h>
#include <qrcodegen.h>
#include <adb/adb.h>

#include "adbc_log.h"
#include "adbc_io.h"
#include "adbc_utils.h"
#include "adbc_server.h"

#define ADBC__QR_PAIR_TIMEOUT 30000 /* ms */
#define CHECK(x, res, label, ...) \
    if(((res) = x) != ADB_ERR_OK) \
    { \
        ADBC_ERROR(__VA_ARGS__); \
        ADBC_INFO("reason: %s", adb_strerror((res))); \
        goto label; \
    }

static int adbc__connect_server(void)
{
    struct sockaddr_in address = {0};
    int fd = -1;
    int result = 0;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0)
    {
        adbc_log_err_errno("failed to create client socket");
        return -1;
    }

    address.sin_family = AF_INET;
    address.sin_port = htons(ADBC_SERVER_PORT);
    result = inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    if(result != 1)
    {
        if(result < 0)
            adbc_log_err_errno("failed to process network address");
        else
            ADBC_ERROR("invalid server network address");
        goto fail;
    }

    if(connect(fd, (const struct sockaddr *)&address, sizeof(address)) != 0)
    {
        adbc_log_err_errno("failed to connect to server");
        goto fail;
    }

    return fd;

fail:
    (void)close(fd);
    return -1;
}

void kill_server_cmd(void *data)
{
    static const char json[] = "{\"type\":\"cmd\",\"cmd\":\"quit\"}";
    int fd = -1;

    (void)data;
    fd = adbc__connect_server();
    if(fd < 0)
        return;

    (void)adbc_send_json_raw(fd, json);
    if(close(fd) != 0)
        adbc_log_err_errno("failed to close client socket");
}

void devices_cmd(void *data)
{
    static const char cmd_json[] = "{\"type\":\"cmd\",\"cmd\":\"devices\"}";
    int server_fd = -1;

    (void)data;
    server_fd = adbc__connect_server();
    if(server_fd < 0)
        return;

    if(!adbc_send_json_raw(server_fd, cmd_json))
        goto cleanup;

    for(;;)
    {
        bool continue_reading = false;
        yyjson_doc *doc = NULL;
        yyjson_val *root = NULL;
        yyjson_val *type = NULL;
        const char *type_str = NULL;

        if(!adbc_recv_json(server_fd, &doc))
        {
            ADBC_ERROR("failed to receive server response");
            goto pkt_cleanup;
        }

        root = yyjson_doc_get_root(doc);
        if(!yyjson_is_obj(root))
        {
            ADBC_ERROR("malformed server JSON");
            goto pkt_cleanup;
        }

        type = yyjson_obj_get(root, "type");
        if(!yyjson_is_str(type))
        {
            ADBC_ERROR("expected server packet type to be a string");
            goto pkt_cleanup;
        }

        type_str = yyjson_get_str(type);
        if(strcmp(type_str, "log") == 0)
        {
            yyjson_val *level = yyjson_obj_get(root, "level");
            yyjson_val *msg = yyjson_obj_get(root, "msg");
            const char *level_str = NULL;
            const char *message = NULL;

            if(!yyjson_is_str(level) || !yyjson_is_str(msg))
            {
                ADBC_ERROR("malformed server log packet");
                goto pkt_cleanup;
            }

            level_str = yyjson_get_str(level);
            message = yyjson_get_str(msg);
            if(strcmp(level_str, "debug") == 0)
                ADBC_DEBUG("%s", message);
            else if(strcmp(level_str, "info") == 0)
                ADBC_INFO("%s", message);
            else if(strcmp(level_str, "warn") == 0)
                ADBC_WARN("%s", message);
            else if(strcmp(level_str, "error") == 0)
                ADBC_ERROR("%s", message);
            else
                ADBC_WARN("unknown server log level: %s", level_str);

            continue_reading = true;
        }
        else if(strcmp(type_str, "result") == 0)
        {
            yyjson_val *devices = yyjson_obj_get(root, "devices");
            yyjson_val *item = NULL;
            size_t idx = 0U;
            size_t max = 0U;

            if(!yyjson_is_arr(devices))
            {
                ADBC_ERROR("expected devices to be an array");
                goto pkt_cleanup;
            }

            yyjson_arr_foreach(devices, idx, max, item)
            {
                if(!yyjson_is_str(item))
                {
                    ADBC_ERROR("invalid device entry in server response");
                    goto pkt_cleanup;
                }
                ADBC_INFO("%s", yyjson_get_str(item));
            }
        }
        else
            ADBC_ERROR("unhandled server packet type: %s", type_str);

pkt_cleanup:
        if(doc != NULL)
            yyjson_doc_free(doc);
        if(!continue_reading)
            break;
    }

cleanup:
    if(close(server_fd) != 0)
        adbc_log_err_errno("failed to close client socket");
}

static bool adbc__display_qr(const char *payload)
{
    uint8_t qrcode[qrcodegen_BUFFER_LEN_MAX];
    uint8_t tmpbuf[qrcodegen_BUFFER_LEN_MAX];
    bool status = false;
    int size = 0;
    int border = 4;

    status = qrcodegen_encodeText(
        payload, tmpbuf, qrcode,
        qrcodegen_Ecc_HIGH,
        qrcodegen_VERSION_MIN,
        qrcodegen_VERSION_MAX,
        qrcodegen_Mask_AUTO,
        true
    );
    if (!status)
        return false;

    size = qrcodegen_getSize(qrcode);

    for (int y = -border; y < size + border; y += 2)
    {
        printf("\x1b[30;47m");
        for (int x = -border; x < size + border; x++)
        {
            bool top = false;
            bool bottom = false;

            if (x >= 0 && x < size && y >= 0 && y < size)
                top = qrcodegen_getModule(qrcode, x, y);

            if (x >= 0 && x < size && y + 1 >= 0 && y + 1 < size)
                bottom = qrcodegen_getModule(qrcode, x, y + 1);

            if (top && bottom)
                printf("\u2588");       /* Both black */
            else if (top)
                printf("\u2580");       /* Black top */
            else if (bottom)
                printf("\u2584");       /* Black bottom */
            else
                printf(" ");            /* Both white */
        }
        printf("\x1b[0m\n");
    }

    return true;
}
static void adbc__client_log_callback(
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

void pair_qr_cmd(void *data)
{
    adbc_cmd_options_t *options = data;
    char service_name[32] = {0};
    char secret[32] = {0};
    char payload[128] = {0};

    adb_wireless_info_t *conn_info = NULL;
    char key_path[PATH_MAX] = {0};

    adb_error_t res = ADB_ERR_OK;
    adb_ctx_t *ctx = NULL;
    adb_conn_t *conn = NULL;
    adb_key_t *key = NULL;

    uint64_t deadline = 0;
    adb_log_set(adbc__client_log_callback, NULL, 
            options->level);

    CHECK(adb_ctx_create(&ctx), 
            res, cleanup, "failed to create libadb context");
    CHECK(adb_pair_qr_build_payload(ctx, 
                service_name, sizeof(service_name),
                secret, sizeof(secret)),
            res, cleanup, "failed to build QR payload");
    CHECK(adb_pair_qr_encode_payload(service_name, secret, 
                payload, sizeof(payload)),
            res, cleanup, "failed to encode QR code");

    adbc__display_qr(payload);

    deadline = adbc_util_monotonic_ms() + ADBC__QR_PAIR_TIMEOUT;
    while(!conn_info)
    {
        res = adb_find_wireless_pairing(ctx, 
                    service_name, &conn_info);

        if(adbc_util_monotonic_ms() > deadline)
        {
            res = ADB_ERR_TIMEOUT;
            ADBC_ERROR("no device was found for %d secs", 30);
            goto cleanup;
        }

        if(res == ADB_ERR_OK)
            break;
        else if(res == ADB_ERR_TIMEOUT || res == ADB_ERR_NOT_FOUND)
            continue;
        else
        {
            ADBC_ERROR("encountered error while finding device");
            ADBC_INFO("reason: %s", adb_strerror(res));
            goto cleanup;
        }
    }

    CHECK(adb_conn_create_wireless_from_info(&conn, ctx, conn_info),
            res, cleanup, "failed to create connection for pairing");


    snprintf(key_path, sizeof(key_path), "%s/%s",
            adbc_util_home_dir(), ".android/adbkey");
    if(adbc_util_file_exist(key_path))
    {
        CHECK(adb_key_load(&key, ctx, key_path), 
                res, cleanup, "failed to load key from \"%s\"", key_path);
    } else {
        ADBC_INFO("no key was found, generating a new one");
        CHECK(adb_key_generate(&key, ctx),
                res, cleanup, "failed to generate new key");
        CHECK(adb_key_save(key, key_path), 
                res, cleanup, "failed to save key to \"%s\"", key_path);
    }


    CHECK(adb_pair_qr(conn, secret, key, NULL, 0),
            res, cleanup, "failed to pair with given device");

cleanup:
    adb_key_destroy(key);
    adb_conn_destroy(conn);
    adb_ctx_destroy(ctx);
}
