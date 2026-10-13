
#include "adbc_server.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <yyjson.h>
#include <adb/adb.h>

#include "adbc_utils.h"
#include "adbc_log.h"
#include "adbc_io.h"
#include "adbc_client.h"
#include "adbc_dynarr.h"

typedef struct
{
    adb_ctx_t *adb_ctx;
    adbc_dynarr_t devices;
    bool quit;
} adbc_server_ctx_t;

typedef const char *adbc__server_error_t;

typedef adbc__server_error_t (*adbc__server_cmd_handler_fn)(
        adbc_server_ctx_t *ctx,
        yyjson_val *req_root,
        yyjson_mut_doc *response_doc,
        yyjson_mut_val *response_root);

static adbc__server_error_t adbc__json_get_str(
        yyjson_val *obj,
        const char *key,
        bool required,
        const char **out)
{
    yyjson_val *child = NULL;

    if(out == NULL)
        return "invalid string output";

    *out = NULL;
    child = obj != NULL ? yyjson_obj_get(obj, key) : NULL;

    if(child == NULL)
    {
        if(required)
            ADBC_ERROR("no object was found with key \"%s\"", key);
        return required ? "missing required string field" : NULL;
    }

    if(!yyjson_is_str(child))
    {
        ADBC_ERROR("object with key \"%s\" was not a string", key);
        return "field must be a string";
    }

    *out = yyjson_get_str(child);
    return NULL;
}

static adbc__server_error_t adbc__cmd_quit(
        adbc_server_ctx_t *ctx,
        yyjson_val *req_root,
        yyjson_mut_doc *response_doc,
        yyjson_mut_val *response_root)
{
    (void)req_root;
    (void)response_doc;
    (void)response_root;

    if(ctx == NULL)
        return "invalid server context";

    ctx->quit = true;
    return NULL;
}

static adbc__server_error_t adbc__cmd_devices(
        adbc_server_ctx_t *ctx,
        yyjson_val *req_root,
        yyjson_mut_doc *response_doc,
        yyjson_mut_val *response_root)
{
    adb_error_t res = ADB_ERR_OK;
    adb_wired_info_t **wired = NULL;
    size_t wired_count = 0U;
    adb_wireless_info_t **wireless = NULL;
    size_t wireless_count = 0U;
    yyjson_mut_val *arr = NULL;
    char buffer[ADB_WIRELESS_INFO_ENDPOINT_SIZE] = {0};

    (void)req_root;

    if(ctx == NULL || response_doc == NULL || response_root == NULL)
        return "invalid devices command arguments";

    arr = yyjson_mut_arr(response_doc);
    if(arr == NULL)
        return "out of memory while creating device list";

    res = adb_query_wired(ctx->adb_ctx, 
            &wired, &wired_count);
    if(res == ADB_ERR_UNSUPPORTED)
        ADBC_WARN("wired ADB devices query is not supported");
    else if(res == ADB_ERR_UNIMPLEMENTED)
        ADBC_WARN("libadb has no implementation for wired query");
    else if(res != ADB_ERR_OK)
        return "failed to query wired ADB devices";

    if(wired_count > 0U && wired == NULL)
        return "ADB wired query returned an invalid device array";

    for(size_t i = 0U; i < wired_count; i++)
    {
        const char *serial = NULL;

        if(wired[i] == NULL ||
                (serial = adb_wired_info_serial(wired[i])) == NULL)
            return "failed to get wired device serial";

        if(!yyjson_mut_arr_add_str(response_doc, arr, serial))
            return "out of memory while adding wired device";
    }

    adb_query_wireless(ctx->adb_ctx, 
            &wireless, &wireless_count);
    if(wireless_count > 0U && wireless == NULL)
        return "ADB wireless query returned an invalid device array";

    for(size_t i = 0U; i < wireless_count; i++)
    {
        if(wireless[i] == NULL)
            return "ADB wireless query returned a null device";

        memset(buffer, 0, sizeof(buffer));
        if(adb_wireless_info_endpoint(wireless[i], 
                    buffer, sizeof(buffer)) != ADB_ERR_OK)
            return "failed to get wireless device endpoint";

        buffer[sizeof(buffer) - 1U] = '\0';
        if(!yyjson_mut_arr_add_str(response_doc, arr, buffer))
            return "out of memory while adding wireless device";
    }

    if(!yyjson_mut_obj_add_val(response_doc, response_root, 
                "devices", arr))
        return "failed to add devices to JSON response";

    return NULL;
}

static adbc__server_error_t adbc__cmd_connect(
        adbc_server_ctx_t *ctx,
        yyjson_val *req_root,
        yyjson_mut_doc *response_doc,
        yyjson_mut_val *response_root)
{
    adbc__server_error_t error = NULL;
    const char *serial = NULL;
    const char *key_path = NULL;
    const char *home_dir = NULL;
    char key_path_buffer[4096] = {0};
    char host[ADBC_IP_LENGTH_MAX] = {0};
    int path_length = 0;
    uint16_t port = 0U;
    adb_key_t *key = NULL;
    adb_error_t res = ADB_ERR_OK;
    adb_conn_t **conn = NULL;

    (void)response_doc;
    (void)response_root;

    if(ctx == NULL)
        return "invalid server context";

    error = adbc__json_get_str(req_root, "serial", 
            true, &serial);
    if(error != NULL)
        return error;

    if(serial[0] == '\0')
        return "field 'serial' must not be empty";

    error = adbc__json_get_str(req_root, "key_path", 
            false, &key_path);
    if(error != NULL)
        return error;

    if(key_path == NULL)
    {
        home_dir = adbc_util_home_dir();
        if(home_dir == NULL)
            return "failed to determine home directory";

        path_length = snprintf(key_path_buffer, sizeof(key_path_buffer),
                "%s/.android/adbkey", home_dir);
        if(path_length < 0 || (size_t)path_length >= sizeof(key_path_buffer))
            return "default ADB key path is too long";

        key_path = key_path_buffer;
    }
    else if(key_path[0] == '\0')
        return "field 'key_path' must not be empty";

    res = adb_key_load(&key, ctx->adb_ctx, key_path);
    if(res != ADB_ERR_OK || key == NULL)
    {
        error = "failed to load ADB private key";
        goto cleanup;
    }

    for(size_t i = 0U; i < ctx->devices.size; i++)
    {
        adb_conn_t **candidate =
            &((adb_conn_t**)ctx->devices.data)[i];

        if(!*candidate)
        {
            conn = candidate;
            break;
        }
    }

    if(!conn)
    {
        if(!adbc_dynarr_push(&ctx->devices, NULL))
        {
            error = "out of memory while allocating device slot";
            goto cleanup;
        }

        conn = &((adb_conn_t**)ctx->devices.data)
            [ctx->devices.size - 1U];
    }

    if(!adbc_util_parse_endpoint(serial, 
                host, sizeof(host), &port))
    {
        error = "serial is not a network endpoint";
        goto cleanup;
    }

    res = adb_conn_create_wireless(conn, 
            ctx->adb_ctx, host, port);

    if(res != ADB_ERR_OK)
    {
        error = "failed to create ADB connection";
        goto cleanup;
    }

    res = adb_handshake(*conn, key);
    if(res != ADB_ERR_OK)
    {
        error = "ADB handshake failed";
        goto cleanup;
    }

cleanup:
    if(key != NULL)
        adb_key_destroy(key);

    if(error && conn)
    {
        adb_conn_destroy(*conn);
        *conn = NULL;
    }
    return error;
}

static bool adbc__handle_cmd(adbc_server_ctx_t *ctx, const int client_fd)
{
    static const struct
    {
        const char *cmd;
        adbc__server_cmd_handler_fn handler;
    } handlers[] =
    {
        { "quit", adbc__cmd_quit },
        { "devices", adbc__cmd_devices },
        { "connect", adbc__cmd_connect }
    };

    bool result = false;
    bool found = false;
    yyjson_doc *req_doc = NULL;
    yyjson_val *req_root = NULL;
    yyjson_mut_doc *response_doc = NULL;
    yyjson_mut_val *response_root = NULL;
    const char *type = NULL;
    const char *cmd = NULL;
    adbc__server_error_t error = NULL;

    if(ctx == NULL || client_fd < 0)
    {
        ADBC_ERROR("invalid command handler arguments");
        return false;
    }

    response_doc = yyjson_mut_doc_new(NULL);
    if(response_doc == NULL)
    {
        ADBC_ERROR("failed to allocate response JSON document");
        goto cleanup;
    }

    response_root = yyjson_mut_obj(response_doc);
    if(response_root == NULL)
    {
        ADBC_ERROR("failed to allocate response JSON object");
        goto cleanup;
    }

    yyjson_mut_doc_set_root(response_doc, response_root);
    if(!yyjson_mut_obj_add_str(response_doc, response_root, 
                "type", "result"))
    {
        ADBC_ERROR("failed to initialize JSON response");
        goto cleanup;
    }

    if(!adbc_recv_json(client_fd, &req_doc))
    {
        error = "failed to receive command frame";
        goto respond;
    }

    req_root = yyjson_doc_get_root(req_doc);
    if(!yyjson_is_obj(req_root))
    {
        ADBC_ERROR("malformed server JSON");
        error = "expected a JSON object";
        goto respond;
    }

    error = adbc__json_get_str(req_root, "type", 
            true, &type);
    if(error != NULL)
        goto respond;

    if(strcmp(type, "cmd") != 0)
    {
        ADBC_ERROR("unexpected packet type: \"%s\"", type);
        error = "unexpected packet type";
        goto respond;
    }

    error = adbc__json_get_str(req_root, "cmd", 
            true, &cmd);
    if(error != NULL)
        goto respond;

    for(size_t i = 0U; i < ADBC_ARRSZ(handlers); i++)
    {
        if(strcmp(cmd, handlers[i].cmd) == 0)
        {
            found = true;
            error = handlers[i].handler(
                    ctx, req_root, response_doc, response_root);
            break;
        }
    }

    if(!found)
    {
        ADBC_ERROR("unknown command: %s", cmd);
        error = "unknown command";
    }

respond:
    if(error != NULL)
    {
        ADBC_WARN("command failed: %s", error);
        if(!yyjson_mut_obj_add_str(response_doc, response_root, 
                    "result", "error") ||
                !yyjson_mut_obj_add_str(response_doc, response_root, 
                    "error", error))
        {
            ADBC_ERROR("failed to construct JSON error response");
            goto cleanup;
        }
    }
    else if(!yyjson_mut_obj_add_str(response_doc, response_root, 
                "result", "success"))
    {
        ADBC_ERROR("failed to construct JSON success response");
        goto cleanup;
    }

    if(!ctx->quit)
    {
        result = adbc_send_json(client_fd, response_doc);
        if(!result)
            ADBC_ERROR("failed to send command response");
    }

cleanup:
    if(req_doc != NULL)
        yyjson_doc_free(req_doc);
    if(response_doc != NULL)
        yyjson_mut_doc_free(response_doc);

    return result;
}

void start_server_cmd(void *data)
{
    adbc_cmd_options_t *options = data;
    struct sockaddr_in address = {0};
    adbc_server_ctx_t ctx = {0};
    int fd = -1;
    int client_fd = -1;
    int option = 1;
    adb_conn_t *conn = NULL;

    if(options == NULL)
    {
        ADBC_ERROR("missing server options");
        return;
    }

    ADBC_INFO("starting server on port %d", ADBC_SERVER_PORT);

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0)
    {
        adbc_log_err_errno("failed to create a new socket");
        return;
    }

    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) != 0)
    {
        adbc_log_err_errno("failed to set socket options");
        goto cleanup;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(ADBC_SERVER_PORT);

    if(bind(fd, (const struct sockaddr *)&address, sizeof(address)) != 0)
    {
        adbc_log_err_errno("failed to bind socket");
        goto cleanup;
    }

    if(listen(fd, 16) != 0)
    {
        adbc_log_err_errno("failed to listen on socket");
        goto cleanup;
    }

    adb_log_set(adbc_log_server_callback, (void *)(intptr_t)-1, options->opt_lvl);
    adb_ctx_create(&ctx.adb_ctx);
    if(ctx.adb_ctx == NULL)
    {
        ADBC_ERROR("failed to create ADB context");
        goto cleanup;
    }

    adbc_dynarr_init(&ctx.devices, sizeof(adb_conn_t*));
    while(!ctx.quit)
    {
        client_fd = accept(fd, NULL, NULL);
        if(client_fd < 0)
        {
            if(errno == EINTR)
                continue;

            adbc_log_err_errno("failed to accept client");
            break;
        }

        adb_log_set_userdata((void *)(intptr_t)client_fd);
        if(!adbc__handle_cmd(&ctx, client_fd))
            ADBC_WARN("failed to handle client command");
        adb_log_set_userdata((void *)(intptr_t)-1);

        if(close(client_fd) != 0)
            adbc_log_err_errno("failed to close client socket");
        client_fd = -1;
    }

    ADBC_INFO("shutting down server on port %d", ADBC_SERVER_PORT);

cleanup:
    adb_log_set_userdata((void *)(intptr_t)-1);
    if(client_fd >= 0)
        (void)close(client_fd);

    adbc_dynarr_foreach(&ctx.devices, adb_conn_t*, conn)
        adb_conn_destroy(conn);
    adbc_dynarr_destroy(&ctx.devices);

    if(ctx.adb_ctx != NULL)
        adb_ctx_destroy(ctx.adb_ctx);
    if(fd >= 0 && close(fd) != 0)
        adbc_log_err_errno("failed to close server socket");
}
