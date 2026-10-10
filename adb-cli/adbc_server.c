#include "adbc_server.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <yyjson.h>
#include <adb/adb_ctx.h>
#include <adb/adb_query.h>

#include "adbc_utils.h"
#include "adbc_log.h"
#include "adbc_io.h"
#include "adbc_client.h"

typedef struct
{
    adb_ctx_t *ctx;
    bool quit;
} adbc_server_ctx_t;

static bool adbc_cmd_quit(adbc_server_ctx_t *ctx, const int client_fd)
{
    (void)client_fd;
    if(ctx == NULL)
        return false;
    ctx->quit = true;
    return true;
}

static bool adbc_cmd_devices(adbc_server_ctx_t *ctx, const int client_fd)
{
    bool result = false;
    adb_wired_info_t **wired = NULL;
    size_t wired_count = 0U;
    adb_wireless_info_t **wireless = NULL;
    size_t wireless_count = 0U;
    yyjson_mut_doc *doc = NULL;
    yyjson_mut_val *root = NULL;
    yyjson_mut_val *arr = NULL;
    char buffer[ADB_WIRELESS_INFO_ENDPOINT_SIZE] = {0};

    if(ctx == NULL || ctx->ctx == NULL || client_fd < 0)
    {
        ADBC_ERROR("invalid devices command context");
        return false;
    }

    doc = yyjson_mut_doc_new(NULL);
    if(doc == NULL)
    {
        ADBC_ERROR("failed to allocate JSON document");
        goto cleanup;
    }

    root = yyjson_mut_obj(doc);
    if(root == NULL)
        goto cleanup;
    yyjson_mut_doc_set_root(doc, root);

    if(!yyjson_mut_obj_add_str(doc, root, "type", "result"))
        goto cleanup;

    arr = yyjson_mut_arr(doc);
    if(arr == NULL)
        goto cleanup;

    /* Consume wired query results before the wireless query can reuse storage. */
    adb_query_wired(ctx->ctx, &wired, &wired_count);
    if(wired_count > 0U && wired == NULL)
    {
        ADBC_ERROR("ADB wired query returned an invalid device array");
        goto cleanup;
    }

    for(size_t i = 0U; i < wired_count; i++)
    {
        const char *serial = NULL;
        if(wired[i] == NULL)
        {
            ADBC_ERROR("ADB wired query returned a null device");
            goto cleanup;
        }

        serial = adb_wired_info_serial(wired[i]);
        if(serial == NULL || !yyjson_mut_arr_add_str(doc, arr, serial))
        {
            ADBC_ERROR("failed to add wired device to JSON result");
            goto cleanup;
        }
    }

    adb_query_wireless(ctx->ctx, &wireless, &wireless_count);
    if(wireless_count > 0U && wireless == NULL)
    {
        ADBC_ERROR("ADB wireless query returned an invalid device array");
        goto cleanup;
    }

    for(size_t i = 0U; i < wireless_count; i++)
    {
        if(wireless[i] == NULL)
        {
            ADBC_ERROR("ADB wireless query returned a null device");
            goto cleanup;
        }

        memset(buffer, 0, sizeof(buffer));
        if(adb_wireless_info_endpoint(wireless[i], buffer, sizeof(buffer)) != ADB_ERR_OK)
        {
            ADBC_ERROR("failed to get wireless device endpoint");
            goto cleanup;
        }

        buffer[sizeof(buffer) - 1U] = '\0';
        if(!yyjson_mut_arr_add_str(doc, arr, buffer))
        {
            ADBC_ERROR("failed to add wireless device to JSON result");
            goto cleanup;
        }
    }

    if(!yyjson_mut_obj_add_val(doc, root, "devices", arr))
        goto cleanup;

    result = adbc_send_json(client_fd, doc);

cleanup:
    if(doc != NULL)
        yyjson_mut_doc_free(doc);
    return result;
}

static bool adbc__handle_cmd(adbc_server_ctx_t *ctx, const int client_fd)
{
    static const struct {
        const char *cmd;
        bool (*handler)(adbc_server_ctx_t *, int);
    } handlers[] =
    {
        { "quit", adbc_cmd_quit },
        { "devices", adbc_cmd_devices }
    };

    bool result = false;
    yyjson_doc *doc = NULL;
    yyjson_val *root = NULL;
    yyjson_val *cmd = NULL;
    yyjson_val *type = NULL;
    const char *cmd_str = NULL;

    if(ctx == NULL || client_fd < 0)
    {
        ADBC_ERROR("invalid command handler arguments");
        return false;
    }

    if(!adbc_recv_json(client_fd, &doc))
    {
        ADBC_ERROR("failed to receive command frame");
        return false;
    }

    root = yyjson_doc_get_root(doc);
    if(!yyjson_is_obj(root))
    {
        ADBC_ERROR("malformed server JSON");
        return false;
    }

    type = yyjson_obj_get(root, "type");
    if(!yyjson_is_str(type))
    {
        ADBC_ERROR("expected type to be a string");
        goto cleanup;
    }

    if(strcmp(yyjson_get_str(type), "cmd") != 0)
    {
        ADBC_ERROR("unexpected packet type");
        ADBC_INFO("expected: \"cmd\", got: \"%s\"", yyjson_get_str(type));
        goto cleanup;
    }

    cmd = yyjson_obj_get(root, "cmd");
    if(!yyjson_is_str(cmd))
    {
        ADBC_ERROR("expected cmd to be a string");
        goto cleanup;
    }

    cmd_str = yyjson_get_str(cmd);
    for(size_t i = 0U; i < ADBC_ARRSZ(handlers); i++)
    {
        if(strcmp(cmd_str, handlers[i].cmd) == 0)
        {
            result = handlers[i].handler(ctx, client_fd);
            goto cleanup;
        }
    }

    ADBC_ERROR("unknown command: %s", cmd_str);

cleanup:
    if(doc != NULL)
        yyjson_doc_free(doc);
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

    if(!options)
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

    adb_log_set(adbc__adb_log_callback, (void *)(intptr_t)-1, options->level);
    adb_ctx_create(&ctx.ctx);
    if(ctx.ctx == NULL)
    {
        ADBC_ERROR("failed to create ADB context");
        goto cleanup;
    }

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
    if(ctx.ctx != NULL)
        adb_ctx_destroy(ctx.ctx);
    if(fd >= 0 && close(fd) != 0)
        adbc_log_err_errno("failed to close server socket");
}
