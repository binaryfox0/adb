
#include "adbc_client.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

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

#define ADBC__QR_PAIR_TIMEOUT 30000U

#define CHECK(x, res, label, ...) \
    do { \
        (res) = (x); \
        if((res) != ADB_ERR_OK) { \
            ADBC_ERROR(__VA_ARGS__); \
            ADBC_INFO("reason: %s", adb_strerror(res)); \
            goto label; \
        } \
    } while(0)

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
            adbc_log_err_errno("failed to process server address");
        else
        {
            ADBC_ERROR("failed to process server address");
            ADBC_INFO("reason: invalid IPv4 address");
        }
        goto cleanup;
    }

    if(connect(fd, (const struct sockaddr *)&address, sizeof(address)) != 0)
    {
        adbc_log_err_errno("failed to connect to server");
        goto cleanup;
    }

    return fd;

cleanup:
    if(close(fd) != 0)
        adbc_log_err_errno("failed to close client socket");
    return -1;
}

static void adbc__close_socket(int *fd)
{
    if(*fd >= 0)
    {
        if(close(*fd) != 0)
            adbc_log_err_errno("failed to close client socket");
        *fd = -1;
    }
}

static bool adbc__get_packet_type(
        yyjson_doc *doc,
        yyjson_val **out_root,
        const char **out_type)
{
    yyjson_val *root = NULL;
    yyjson_val *type = NULL;

    root = yyjson_doc_get_root(doc);
    if(!yyjson_is_obj(root))
    {
        ADBC_ERROR("malformed server JSON");
        ADBC_INFO("reason: expected a JSON object");
        return false;
    }

    type = yyjson_obj_get(root, "type");
    if(!yyjson_is_str(type))
    {
        ADBC_ERROR("malformed server packet");
        ADBC_INFO("reason: expected packet type to be a string");
        return false;
    }

    *out_root = root;
    *out_type = yyjson_get_str(type);
    return true;
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
    if(!status)
    {
        ADBC_ERROR("failed to generate QR code");
        ADBC_INFO("reason: payload could not be encoded");
        return false;
    }

    size = qrcodegen_getSize(qrcode);

    for(int y = -border; y < size + border; y += 2)
    {
        printf("\x1b[30;47m");
        for(int x = -border; x < size + border; x++)
        {
            bool top = false;
            bool bottom = false;

            if(x >= 0 && x < size && y >= 0 && y < size)
                top = qrcodegen_getModule(qrcode, x, y);

            if(x >= 0 && x < size && y + 1 >= 0 && y + 1 < size)
                bottom = qrcodegen_getModule(qrcode, x, y + 1);

            if(top && bottom)
                printf("\u2588");
            else if(top)
                printf("\u2580");
            else if(bottom)
                printf("\u2584");
            else
                printf(" ");
        }
        printf("\x1b[0m\n");
    }

    return true;
}

static adb_key_t *adbc__key_ensure_load(
        adb_ctx_t *ctx,
        const char *path)
{
    adb_error_t res = ADB_ERR_OK;
    adb_key_t *key = NULL;

    if(adbc_util_file_exist(path))
    {
        ADBC_INFO("loading key from \"%s\"", path);
        CHECK(adb_key_load(&key, ctx, path),
                res, cleanup, "failed to load key from \"%s\"", path);
    }
    else
    {
        ADBC_INFO("generating new key");
        CHECK(adb_key_generate(&key, ctx),
                res, cleanup, "failed to generate key");
        CHECK(adb_key_save(key, path),
                res, cleanup, "failed to save key to \"%s\"", path);
    }

    return key;

cleanup:
    adb_key_destroy(key);
    return NULL;
}

void kill_server_cmd(void *data)
{
    static const char json[] = "{\"type\":\"cmd\",\"cmd\":\"quit\"}";
    int fd = -1;

    (void)data;

    fd = adbc__connect_server();
    if(fd < 0)
        return;

    if(!adbc_send_json_raw(fd, json))
    {
        ADBC_ERROR("failed to send quit command");
        ADBC_INFO("reason: JSON send operation failed");
    }

    adbc__close_socket(&fd);
}

void devices_cmd(void *data)
{
    static const char json[] = "{\"type\":\"cmd\",\"cmd\":\"devices\"}";
    int fd = -1;
    bool done = false;

    (void)data;

    fd = adbc__connect_server();
    if(fd < 0)
        return;

    if(!adbc_send_json_raw(fd, json))
    {
        ADBC_ERROR("failed to send devices command");
        ADBC_INFO("reason: JSON send operation failed");
        goto cleanup;
    }

    while(!done)
    {
        yyjson_doc *doc = NULL;
        yyjson_val *root = NULL;
        const char *type = NULL;

        if(!adbc_recv_json(fd, &doc))
        {
            ADBC_ERROR("failed to receive server response");
            ADBC_INFO("reason: server disconnected or JSON read failed");
            yyjson_doc_free(doc);
            goto cleanup;
        }

        if(!adbc__get_packet_type(doc, &root, &type))
        {
            yyjson_doc_free(doc);
            goto cleanup;
        }

        if(strcmp(type, "log") == 0)
            adbc_log_handle_server(root);
        else if(strcmp(type, "result") == 0)
        {
            yyjson_val *devices = yyjson_obj_get(root, "devices");
            yyjson_val *item = NULL;
            size_t index = 0U;
            size_t count = 0U;

            if(!yyjson_is_arr(devices))
            {
                ADBC_ERROR("invalid devices response");
                ADBC_INFO("reason: expected devices to be an array");
                yyjson_doc_free(doc);
                goto cleanup;
            }

            yyjson_arr_foreach(devices, index, count, item)
            {
                if(!yyjson_is_str(item))
                {
                    ADBC_ERROR("invalid device entry");
                    ADBC_INFO("reason: expected device entry to be a string");
                    yyjson_doc_free(doc);
                    goto cleanup;
                }
                ADBC_INFO("%s", yyjson_get_str(item));
            }

            done = true;
        }
        else
            ADBC_ERROR("unhandled server packet type: %s", type);

        yyjson_doc_free(doc);
    }

cleanup:
    adbc__close_socket(&fd);
}

void pair_cmd(void *data)
{
    adbc_cmd_options_t *options = data;
    char host[INET6_ADDRSTRLEN] = {0};
    uint16_t port = 0U;
    adb_error_t res = ADB_ERR_OK;
    adb_ctx_t *ctx = NULL;
    adb_conn_t *conn = NULL;
    adb_key_t *key = NULL;

    adb_log_set(adbc_log_client_callback, NULL, options->opt_lvl);

    if(!adbc_util_parse_endpoint(options->endpoint, host, sizeof(host), &port))
    {
        ADBC_ERROR("failed to parse endpoint \"%s\"", options->endpoint);
        ADBC_INFO("reason: expected a valid host:port endpoint");
        goto cleanup;
    }

    CHECK(adb_ctx_create(&ctx),
            res, cleanup, "failed to create ADB context");
    CHECK(adb_conn_create_wireless(&conn, ctx, host, port),
            res, cleanup, "failed to create wireless pairing connection");

    key = adbc__key_ensure_load(ctx, options->opt_key);
    if(!key)
        goto cleanup;

    CHECK(adb_pair(conn, options->code, key, NULL, 0),
            res, cleanup, "failed to pair with device");

    ADBC_INFO("paired with \"%s:%u\" successfully", host, (unsigned int)port);

cleanup:
    adb_key_destroy(key);
    adb_conn_destroy(conn);
    adb_ctx_destroy(ctx);
}

void pair_qr_cmd(void *data)
{
    adbc_cmd_options_t *options = data;
    char service_name[32] = {0};
    char secret[32] = {0};
    char payload[128] = {0};
    adb_wireless_info_t *conn_info = NULL;
    adb_error_t res = ADB_ERR_OK;
    adb_ctx_t *ctx = NULL;
    adb_conn_t *conn = NULL;
    adb_key_t *key = NULL;
    uint64_t deadline = 0U;
    uint64_t now = 0U;

    adb_log_set(adbc_log_client_callback, NULL, options->opt_lvl);

    CHECK(adb_ctx_create(&ctx),
            res, cleanup, "failed to create ADB context");
    CHECK(adb_pair_qr_build_payload(ctx,
                service_name, sizeof(service_name),
                secret, sizeof(secret)),
            res, cleanup, "failed to build QR pairing payload");
    CHECK(adb_pair_qr_encode_payload(service_name, secret,
                payload, sizeof(payload)),
            res, cleanup, "failed to encode QR pairing payload");

    if(!adbc__display_qr(payload))
        goto cleanup;

    ADBC_INFO("waiting up to %u seconds for pairing device",
            ADBC__QR_PAIR_TIMEOUT / 1000U);
    deadline = adbc_util_monotonic_ms() + ADBC__QR_PAIR_TIMEOUT;

    while(conn_info == NULL)
    {
        now = adbc_util_monotonic_ms();
        if(now >= deadline)
        {
            ADBC_ERROR("timed out waiting for pairing device");
            ADBC_INFO("reason: no device found within %u seconds",
                    ADBC__QR_PAIR_TIMEOUT / 1000U);
            goto cleanup;
        }

        res = adb_find_wireless_pairing(ctx, service_name, &conn_info);
        if(res == ADB_ERR_OK)
            break;

        if(res != ADB_ERR_TIMEOUT && res != ADB_ERR_NOT_FOUND)
        {
            ADBC_ERROR("failed to discover pairing device");
            ADBC_INFO("reason: %s", adb_strerror(res));
            goto cleanup;
        }
    }

    CHECK(adb_conn_create_wireless_from_info(&conn, ctx, conn_info),
            res, cleanup, "failed to create QR pairing connection");

    key = adbc__key_ensure_load(ctx, options->opt_key);
    if(!key)
        goto cleanup;

    CHECK(adb_pair_qr(conn, secret, key, NULL, 0),
            res, cleanup, "failed to pair with QR code");

    ADBC_INFO("paired with device successfully");

cleanup:
    adb_key_destroy(key);
    adb_conn_destroy(conn);
    adb_ctx_destroy(ctx);
}

void connect_cmd(void *data)
{
    adbc_cmd_options_t *options = data;
    int fd = -1;
    yyjson_mut_doc *doc = NULL;
    yyjson_mut_val *root = NULL;
    bool done = false;

    adb_log_set(adbc_log_client_callback, NULL, options->opt_lvl);

    fd = adbc__connect_server();
    if(fd < 0)
        goto cleanup;

    doc = yyjson_mut_doc_new(NULL);
    if(!doc)
    {
        ADBC_ERROR("failed to allocate connect command");
        ADBC_INFO("reason: out of memory");
        goto cleanup;
    }

    root = yyjson_mut_obj(doc);
    if(!root)
    {
        ADBC_ERROR("failed to create connect command");
        ADBC_INFO("reason: out of memory");
        goto cleanup;
    }

    yyjson_mut_doc_set_root(doc, root);
    if(!yyjson_mut_obj_add_str(doc, root, "type", "cmd") ||
       !yyjson_mut_obj_add_str(doc, root, "cmd", "connect") ||
       !yyjson_mut_obj_add_str(doc, root, "serial", options->serial))
    {
        ADBC_ERROR("failed to build connect command");
        ADBC_INFO("reason: JSON allocation failed");
        goto cleanup;
    }

    if(!adbc_send_json(fd, doc))
    {
        ADBC_ERROR("failed to send connect command");
        ADBC_INFO("reason: JSON send operation failed");
        goto cleanup;
    }

    yyjson_mut_doc_free(doc);
    doc = NULL;

    while(!done)
    {
        yyjson_doc *response = NULL;
        yyjson_val *response_root = NULL;
        const char *type = NULL;

        if(!adbc_recv_json(fd, &response))
        {
            ADBC_ERROR("failed to receive connect response");
            ADBC_INFO("reason: server disconnected or JSON read failed");
            yyjson_doc_free(response);
            goto cleanup;
        }

        if(!adbc__get_packet_type(response, &response_root, &type))
        {
            yyjson_doc_free(response);
            goto cleanup;
        }

        if(strcmp(type, "log") == 0)
            adbc_log_handle_server(response_root);
        else if(strcmp(type, "result") == 0)
        {
            yyjson_val *error = yyjson_obj_get(response_root, "error");
            yyjson_val *message = yyjson_obj_get(response_root, "message");

            if(yyjson_is_bool(error) && yyjson_get_bool(error))
            {
                ADBC_ERROR("failed to connect to \"%s\"", options->serial);
                if(yyjson_is_str(message))
                    ADBC_INFO("reason: %s", yyjson_get_str(message));
                else
                    ADBC_INFO("reason: server rejected connect command");
            }
            else
                ADBC_INFO("connected to \"%s\" successfully", options->serial);

            done = true;
        }
        else
            ADBC_ERROR("unhandled server packet type: %s", type);

        yyjson_doc_free(response);
    }

cleanup:
    yyjson_mut_doc_free(doc);
    adbc__close_socket(&fd);
}
