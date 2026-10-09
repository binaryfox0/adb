#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <aparse.h>
#include <adb/adb.h>
#include <yyjson.h>

#define debug aparse_prog_debug
#define info aparse_prog_info
#define warn aparse_prog_warn
#define error aparse_prog_error

#define ADBC_SERVER_PORT 9000
#define ADBC__MAX_JSON_SIZE (8U * 1024U)
#define ADBC_ARRSZ(arr) (sizeof(arr) / sizeof((arr)[0]))

#ifdef MSG_NOSIGNAL
#   define ADBC__SEND_FLAGS MSG_NOSIGNAL
#else
#   define ADBC__SEND_FLAGS 0
#endif

#if defined(__clang__) || defined(__GNUC__)
#   define ADB__PRINTF(fmt_index, arg_index) \
        __attribute__((format(printf, fmt_index, arg_index)))
#   define ADB__PRINTF_FMT
#elif defined(_MSC_VER)
#   define ADB__PRINTF(fmt_index, arg_index)
#   define ADB__PRINTF_FMT _Printf_format_string_
#else
#   define ADB__PRINTF(fmt_index, arg_index)
#   define ADB__PRINTF_FMT
#endif

static ADB__PRINTF(1, 2) void adbc_log_err_errno(
        ADB__PRINTF_FMT const char *fmt, ...)
{
    int saved_errno = errno;
    char buffer[1024] = {0};
    va_list va;

    if(fmt == NULL)
        return;

    va_start(va, fmt);
    (void)vsnprintf(buffer, sizeof(buffer), fmt, va);
    va_end(va);

    error("%s", buffer);
    info("reason: %s", strerror(saved_errno));
}

static bool adbc__send(
        const int fd,
        const void *data,
        const size_t size)
{
    const uint8_t *bytes = data;
    size_t offset = 0U;

    if(fd < 0 || (size > 0U && data == NULL))
    {
        error("invalid send arguments");
        return false;
    }

    while(offset < size)
    {
        ssize_t result = send(fd, bytes + offset,
                size - offset, ADBC__SEND_FLAGS);

        if(result < 0)
        {
            if(errno == EINTR)
                continue;

            adbc_log_err_errno("failed to send data");
            return false;
        }

        if(result == 0)
        {
            error("socket closed while sending data");
            return false;
        }

        offset += (size_t)result;
    }

    return true;
}

static bool adbc__recv(
        const int fd,
        void *data,
        const size_t size)
{
    uint8_t *bytes = data;
    size_t offset = 0U;

    if(fd < 0 || (size > 0U && data == NULL))
    {
        error("invalid receive arguments");
        return false;
    }

    while(offset < size)
    {
        ssize_t result = recv(fd, bytes + offset, size - offset, 0);

        if(result < 0)
        {
            if(errno == EINTR)
                continue;

            adbc_log_err_errno("failed to receive data");
            return false;
        }

        if(result == 0)
            return false;

        offset += (size_t)result;
    }

    return true;
}

static bool adbc__send_json(
        const int fd,
        const char *json,
        const size_t json_len)
{
    uint16_t length_be = 0U;

    if(fd < 0 || json == NULL || json_len == 0U ||
            json_len > ADBC__MAX_JSON_SIZE)
    {
        error("invalid JSON send arguments or payload size");
        return false;
    }

    length_be = htons((uint16_t)json_len);

    return adbc__send(fd, &length_be, sizeof(length_be)) &&
        adbc__send(fd, json, json_len);
}

static bool adbc__recv_json(
        const int fd,
        char **out_json,
        size_t *out_size)
{
    uint16_t length_be = 0U;
    size_t length = 0U;
    char *json = NULL;

    if(out_json != NULL)
        *out_json = NULL;
    if(out_size != NULL)
        *out_size = 0U;

    if(fd < 0 || out_json == NULL || out_size == NULL)
    {
        error("invalid JSON receive arguments");
        return false;
    }

    if(!adbc__recv(fd, &length_be, sizeof(length_be)))
        return false;

    length = (size_t)ntohs(length_be);
    if(length == 0U || length > ADBC__MAX_JSON_SIZE)
    {
        error("invalid JSON frame length: %zu", length);
        return false;
    }

    json = malloc(length + 1U);
    if(json == NULL)
    {
        error("failed to allocate JSON receive buffer");
        return false;
    }

    if(!adbc__recv(fd, json, length))
    {
        free(json);
        return false;
    }

    json[length] = '\0';
    *out_json = json;
    *out_size = length;
    return true;
}

typedef struct
{
    adb_ctx_t *ctx;
    bool quit;
} adbc_server_ctx_t;

typedef bool (*adbc_cmd_handler_fn)(
        adbc_server_ctx_t *ctx,
        int client_fd);

static bool adbc_cmd_quit(
        adbc_server_ctx_t *ctx,
        int client_fd)
{
    (void)client_fd;

    if(ctx == NULL)
        return false;

    ctx->quit = true;
    return true;
}

static bool adbc_cmd_devices(
        adbc_server_ctx_t *ctx,
        int client_fd)
{
    bool res = false;
    adb_wired_info_t **wired = NULL;
    size_t wired_count = 0U;
    adb_wireless_info_t **wireless = NULL;
    size_t wireless_count = 0U;
    yyjson_mut_doc *doc = NULL;
    yyjson_mut_val *root = NULL;
    yyjson_mut_val *arr = NULL;
    char *json = NULL;
    size_t json_len = 0U;
    char buffer[ADB_WIRELESS_INFO_ENDPOINT_SIZE] = {0};

    if(ctx == NULL || ctx->ctx == NULL || client_fd < 0)
    {
        error("invalid devices command context");
        return false;
    }

    adb_query_wired(ctx->ctx, &wired, &wired_count);
    adb_query_wireless(ctx->ctx, &wireless, &wireless_count);

    if((wired_count > 0U && wired == NULL) ||
            (wireless_count > 0U && wireless == NULL))
    {
        error("ADB query returned an invalid device array");
        goto cleanup;
    }

    doc = yyjson_mut_doc_new(NULL);
    if(doc == NULL)
    {
        error("failed to allocate JSON document");
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

    for(size_t i = 0U; i < wired_count; i++)
    {
        const char *serial = NULL;

        if(wired[i] == NULL)
        {
            error("ADB query returned a null wired device");
            goto cleanup;
        }

        serial = adb_wired_info_serial(wired[i]);
        if(serial == NULL ||
                !yyjson_mut_arr_add_str(doc, arr, serial))
        {
            error("failed to add wired device to JSON result");
            goto cleanup;
        }
    }

    for(size_t i = 0U; i < wireless_count; i++)
    {
        if(wireless[i] == NULL)
        {
            error("ADB query returned a null wireless device");
            goto cleanup;
        }

        memset(buffer, 0, sizeof(buffer));

        if(adb_wireless_info_endpoint(
                    wireless[i], 
                    buffer, sizeof(buffer)) != ADB_ERR_OK)
        {
            error("failed to get wireless device endpoint");
            goto cleanup;
        }

        buffer[sizeof(buffer) - 1U] = '\0';
        if(!yyjson_mut_arr_add_str(doc, arr, buffer))
        {
            error("failed to add wireless device to JSON result");
            goto cleanup;
        }
    }

    if(!yyjson_mut_obj_add_val(doc, root, "devices", arr))
        goto cleanup;

    json = yyjson_mut_write(doc, 0, &json_len);
    if(json == NULL)
    {
        error("failed to serialize devices JSON");
        goto cleanup;
    }

    res = adbc__send_json(client_fd, json, json_len);

cleanup:
    free(json);
    yyjson_mut_doc_free(doc);
    return res;
}

static bool adbc__handle_cmd(
        adbc_server_ctx_t *ctx,
        int client_fd)
{
    static const struct {
        const char *cmd;
        adbc_cmd_handler_fn handler;
    } handlers[] =
    {
        { "quit",    adbc_cmd_quit },
        { "devices", adbc_cmd_devices }
    };

    bool res = false;
    char *json = NULL;
    size_t json_len = 0U;
    yyjson_read_err read_err = {0};
    yyjson_doc *doc = NULL;
    yyjson_val *root = NULL;
    yyjson_val *cmd = NULL;
    yyjson_val *type = NULL;
    const char *cmd_str = NULL;

    if(ctx == NULL || client_fd < 0)
    {
        error("invalid command handler arguments");
        return false;
    }

    if(!adbc__recv_json(client_fd, &json, &json_len))
    {
        error("failed to receive command frame");
        return false;
    }

    doc = yyjson_read_opts(json, json_len, 0, NULL, &read_err);
    if(doc == NULL)
    {
        error("failed to parse JSON payload");
        info("reason: %s, at %zu byte",
                read_err.msg != NULL ? read_err.msg : "unknown error",
                read_err.pos);
        goto cleanup;
    }

    root = yyjson_doc_get_root(doc);
    if(!yyjson_is_obj(root))
    {
        error("malformed JSON payload");
        goto cleanup;
    }

    type = yyjson_obj_get(root, "type");
    if(!yyjson_is_str(type))
    {
        error("expected type to be a string");
        goto cleanup;
    }

    if(strcmp(yyjson_get_str(type), "cmd") != 0)
    {
        error("unexpected packet type");
        info("expected: \"cmd\", got: \"%s\"", yyjson_get_str(type));
        goto cleanup;
    }

    cmd = yyjson_obj_get(root, "cmd");
    if(!yyjson_is_str(cmd))
    {
        error("expected cmd to be a string");
        goto cleanup;
    }

    cmd_str = yyjson_get_str(cmd);

    for(size_t i = 0U; i < ADBC_ARRSZ(handlers); i++)
    {
        if(strcmp(cmd_str, handlers[i].cmd) == 0)
        {
            res = handlers[i].handler(ctx, client_fd);
            goto cleanup;
        }
    }

    error("unknown command: %s", cmd_str);

cleanup:
    yyjson_doc_free(doc);
    free(json);
    return res;
}

static const char *const adbc__log_level_strs[ADB__LOG_COUNT] =
{
    [ADB_LOG_DEBUG] = "debug",
    [ADB_LOG_INFO]  = "info",
    [ADB_LOG_WARN]  = "warn",
    [ADB_LOG_ERROR] = "error"
};

static void adbc__adb_log_callback(
        void *userdata,
        adb_log_level_t level,
        const char *msg)
{
    intptr_t log_fd = (intptr_t)userdata;
    int fd = -1;
    const char *message = msg != NULL ? msg : "";
    yyjson_mut_doc *doc = NULL;
    yyjson_mut_val *root = NULL;
    char *json = NULL;
    size_t json_len = 0U;

    if(log_fd < 0)
    {
        switch(level)
        {
            case ADB_LOG_DEBUG: debug("%s", message); break;
            case ADB_LOG_INFO:  info("%s", message); break;
            case ADB_LOG_WARN:  warn("%s", message); break;
            case ADB_LOG_ERROR: error("%s", message); break;
            case ADB__LOG_COUNT:
            default:
                warn("unknown ADB log level %d: %s",
                        (int)level, message);
                break;
        }
        return;
    }

    if(log_fd > INT_MAX ||
            (unsigned)level >= (unsigned)ADB__LOG_COUNT ||
            adbc__log_level_strs[(size_t)level] == NULL)
        return;

    fd = (int)log_fd;

    doc = yyjson_mut_doc_new(NULL);
    if(doc == NULL)
        return;

    root = yyjson_mut_obj(doc);
    if(root == NULL)
        goto cleanup;

    yyjson_mut_doc_set_root(doc, root);

    if(!yyjson_mut_obj_add_str(doc, root, "type", "log") ||
            !yyjson_mut_obj_add_str(
                doc, root, "level", 
                adbc__log_level_strs[(size_t)level]) ||
            !yyjson_mut_obj_add_str(doc, root, "msg", message))
        goto cleanup;

    json = yyjson_mut_write(doc, 0, &json_len);
    if(json == NULL)
        goto cleanup;

    (void)adbc__send_json(fd, json, json_len);

cleanup:
    free(json);
    yyjson_mut_doc_free(doc);
}

typedef struct
{
    adb_log_level_t level;
} adbc_cmd_options_t;

static void start_server_cmd(
        const aparse_arg *arg,
        void *data)
{
    adbc_cmd_options_t *options = data;
    struct sockaddr_in address = {0};
    adbc_server_ctx_t ctx = {0};
    int fd = -1;
    int client_fd = -1;
    int option = 1;

    (void)arg;

    if(options == NULL)
    {
        error("missing server options");
        return;
    }

    info("starting server on port %d", ADBC_SERVER_PORT);
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0)
    {
        adbc_log_err_errno("failed to create a new socket");
        return;
    }

    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                &option, sizeof(option)) != 0)
    {
        adbc_log_err_errno("failed to set socket options");
        goto cleanup;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(ADBC_SERVER_PORT);

    if(bind(fd, (const struct sockaddr *)&address,
                sizeof(address)) != 0)
    {
        adbc_log_err_errno("failed to bind socket");
        goto cleanup;
    }

    if(listen(fd, 16) != 0)
    {
        adbc_log_err_errno("failed to listen on socket");
        goto cleanup;
    }

    adb_log_set(adbc__adb_log_callback,
            (void *)(intptr_t)-1, options->level);

    adb_ctx_create(&ctx.ctx);
    if(ctx.ctx == NULL)
    {
        error("failed to create ADB context");
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
            warn("failed to handle client command");

        adb_log_set_userdata((void *)(intptr_t)-1);

        if(close(client_fd) != 0)
            adbc_log_err_errno("failed to close client socket");

        client_fd = -1;
    }

    info("shutting down server on port %d", ADBC_SERVER_PORT);

cleanup:
    adb_log_set_userdata((void *)(intptr_t)-1);

    if(client_fd >= 0)
        (void)close(client_fd);

    if(ctx.ctx != NULL)
        adb_ctx_destroy(ctx.ctx);

    if(fd >= 0 && close(fd) != 0)
        adbc_log_err_errno("failed to close server socket");
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
            error("invalid server network address");

        goto fail;
    }

    if(connect(fd, (const struct sockaddr *)&address,
                sizeof(address)) != 0)
    {
        adbc_log_err_errno("failed to connect to server");
        goto fail;
    }

    return fd;

fail:
    (void)close(fd);
    return -1;
}

static void kill_server_cmd(
        const aparse_arg *arg,
        void *data)
{
    static const char json[] = "{\"type\":\"cmd\",\"cmd\":\"quit\"}";
    int fd = -1;

    (void)arg;
    (void)data;

    fd = adbc__connect_server();
    if(fd < 0)
        return;

    (void)adbc__send_json(fd, json, sizeof(json) - 1U);

    if(close(fd) != 0)
        adbc_log_err_errno("failed to close client socket");
}

static void devices_cmd(
        const aparse_arg *arg,
        void *data)
{
    static const char cmd_json[] =
        "{\"type\":\"cmd\",\"cmd\":\"devices\"}";

    int server_fd = -1;

    (void)arg;
    (void)data;

    server_fd = adbc__connect_server();
    if(server_fd < 0)
        return;

    if(!adbc__send_json(server_fd, cmd_json, sizeof(cmd_json) - 1U))
        goto cleanup;

    for(;;)
    {
        bool continue_reading = false;
        char *json = NULL;
        size_t json_len = 0U;
        yyjson_read_err read_err = {0};
        yyjson_doc *doc = NULL;
        yyjson_val *root = NULL;
        yyjson_val *type = NULL;
        const char *type_str = NULL;

        if(!adbc__recv_json(server_fd, &json, &json_len))
        {
            error("failed to receive server response");
            goto packet_cleanup;
        }

        doc = yyjson_read_opts(json, json_len, 0, NULL, &read_err);
        if(doc == NULL)
        {
            error("failed to parse server JSON");
            info("reason: %s, at %zu byte",
                    read_err.msg != NULL ? read_err.msg : "unknown error",
                    read_err.pos);
            goto packet_cleanup;
        }

        root = yyjson_doc_get_root(doc);
        if(!yyjson_is_obj(root))
        {
            error("malformed server JSON");
            goto packet_cleanup;
        }

        type = yyjson_obj_get(root, "type");
        if(!yyjson_is_str(type))
        {
            error("expected server packet type to be a string");
            goto packet_cleanup;
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
                error("malformed server log packet");
                goto packet_cleanup;
            }

            level_str = yyjson_get_str(level);
            message = yyjson_get_str(msg);

            if(strcmp(level_str, "debug") == 0)
                debug("%s", message);
            else if(strcmp(level_str, "info") == 0)
                info("%s", message);
            else if(strcmp(level_str, "warn") == 0)
                warn("%s", message);
            else if(strcmp(level_str, "error") == 0)
                error("%s", message);
            else
                warn("unknown server log level: %s", level_str);

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
                error("expected devices to be an array");
                goto packet_cleanup;
            }

            yyjson_arr_foreach(devices, idx, max, item)
            {
                if(!yyjson_is_str(item))
                {
                    error("invalid device entry in server response");
                    goto packet_cleanup;
                }

                info("%s", yyjson_get_str(item));
            }
        }
        else
        {
            error("unhandled server packet type: %s", type_str);
        }

packet_cleanup:
        yyjson_doc_free(doc);
        free(json);

        if(!continue_reading)
            break;
    }

cleanup:
    if(close(server_fd) != 0)
        adbc_log_err_errno("failed to close client socket");
}

int main(int argc, char **argv)
{
    const char *level_str = NULL;
    adbc_cmd_options_t options = {0};
    aparse_list dispatch = {0};

    aparse_arg commands[] =
    {
        aparse_arg_subparser(
                "start-server", NULL,
                start_server_cmd, &options, sizeof(options),
                "Ensure that there is a server running"),
        aparse_arg_subparser(
                "kill-server", NULL,
                kill_server_cmd, &options, sizeof(options),
                "Kill the server if it is running"),
        aparse_arg_subparser(
                "devices", NULL,
                devices_cmd, &options, sizeof(options),
                "List connected devices"),
        aparse_arg_end_marker
    };

    aparse_arg main_args[] =
    {
        aparse_arg_parser("command", commands),
        aparse_arg_option(
                "-lvl", "--log-level",
                &level_str, 0, APARSE_ARG_TYPE_STRING,
                "Set libadb and adb-cli log level"),
        aparse_arg_end_marker
    };

    if(aparse_parse(argc, argv, main_args, &dispatch,
                "libadb command line utils") != APARSE_STATUS_OK)
        return 1;

    if(level_str != NULL)
    {
        options.level = ADB__LOG_COUNT;

        for(size_t i = 0U; i < ADBC_ARRSZ(adbc__log_level_strs); i++)
        {
            if(adbc__log_level_strs[i] != NULL &&
                    strcmp(adbc__log_level_strs[i], level_str) == 0)
            {
                options.level = (adb_log_level_t)i;
                break;
            }
        }

        if(options.level == ADB__LOG_COUNT)
        {
            error("unknown log level was specified");
            info("supported: \"debug\", \"info\", \"warn\", \"error\", "
                    "got: \"%s\"", level_str);
            return 1;
        }
    }
    else
        options.level = ADB_LOG_ERROR;

    aparse_dispatch_all(&dispatch);
    return 0;
}
