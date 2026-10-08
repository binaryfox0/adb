#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <aparse.h>
#include <adb/adb.h>
#include <yyjson.h>

#define debug aparse_prog_debug
#define info aparse_prog_info
#define warn aparse_prog_warn
#define error aparse_prog_error

#define ADBC_SERVER_PORT 9000
#define ADBC_ARRSZ(arr) (sizeof((arr)) / sizeof((arr)[0]))

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
        ADB__PRINTF_FMT const char *fmt, 
        ...)
{
    va_list va;
    char buffer[1024] = {0};
    if(!fmt)
        return;

    va_start(va, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, va);
    va_end(va);

    error("%s", buffer);
    info("reason: %s", strerror(errno));
}

static bool adbc__send(
        const int fd,
        const void *data,
        const size_t size)
{
    size_t offset = 0;
    while(offset < size)
    {
        ssize_t res = send(fd, 
                (const uint8_t*)data + offset, 
                size - offset, 0);
        if(res < 0)
        {
            if(errno == EINTR)
                continue;

            adbc_log_err_errno("failed to send data");
            return false;
        }

        if(res == 0)
            return false;
        offset += (size_t)res;
    }
    return true;
}

static int adbc__recv(
        const int fd,
        void *data,
        const size_t size)
{
    size_t offset = 0;
    while(offset < size)
    {
        ssize_t res = recv(fd, 
                (uint8_t*)data + offset, 
                size - offset, 0);
        if(res < 0)
        {
            if(errno == EINTR)
                continue;
            adbc_log_err_errno("failed to recieve data");
            return false;
        }

        if(res == 0)
            return false;
        offset += (size_t)res;
    }
    return true;
}
#define ADBC__MAX_JSON_SIZE (8 * 1024)
static bool adbc__send_json(
        const int fd,
        const char *json,
        const size_t json_len)
{
    uint16_t length = 0U;
    if(fd < 0 || json == NULL ||
            json_len > ADBC__MAX_JSON_SIZE)
        return false;

    length = htons((uint16_t)json_len);
    if(!adbc__send(fd, &length, sizeof(length)))
        return false;
    if(!adbc__send(fd, json, json_len))
        return false;

    return true;
}

static bool adbc__recv_json(
        const int fd,
        char **out_json,
        size_t *out_size)
{
    uint16_t length_be = 0U;
    uint16_t length = 0U;
    char *json = NULL;

    if(fd < 0 || out_json == NULL || out_size == NULL)
        return false;

    if(!adbc__recv(fd, &length_be, sizeof(length_be)))
        return false;

    length = ntohs(length_be);
    if(length > ADBC__MAX_JSON_SIZE)
        return false;

    json = malloc(length);
    if(!json)
        return false;

    if(!adbc__recv(fd, json, (size_t)length))
    {
        free(json);
        return false;
    }

    *out_json = json;
    *out_size = (size_t)length;
    return true;
}

typedef struct
{
    adb_ctx_t *ctx;
    bool quit;
} adbc_server_ctx_t;

typedef bool (*adbc_cmd_handler_fn)(
        adbc_server_ctx_t *ctx,
        const int client_fd);

static bool adbc_cmd_quit(
        adbc_server_ctx_t *ctx,
        const int client_fd)
{
    (void)client_fd;
    ctx->quit = true;
    return true;
}

static bool adbc_cmd_devices(
        adbc_server_ctx_t *ctx,
        const int client_fd)
{
    bool res = false;
    adb_wired_info_t **wired = NULL;
    size_t wired_count = 0;
    adb_wireless_info_t **wireless = NULL;
    size_t wireless_count = 0;
    yyjson_mut_doc *doc = NULL;
    yyjson_mut_val *root = NULL;
    yyjson_mut_val *arr = NULL;
    char *json = NULL;
    size_t json_len = 0;
    char buffer[ADB_WIRELESS_INFO_ENDPOINT_SIZE] = {0};

    adb_query_wired(ctx->ctx,
            &wired, &wired_count);
    adb_query_wireless(ctx->ctx,
            &wireless, &wireless_count);

    doc = yyjson_mut_doc_new(NULL);
    if(!doc)
        return false;

    root = yyjson_mut_obj(doc);
    if(!root)
        goto cleanup;

    yyjson_mut_doc_set_root(doc, root);

    if(!yyjson_mut_obj_add_str(doc, root, "type", "result"))
        goto cleanup;

    arr = yyjson_mut_arr(doc);
    if(!arr)
        goto cleanup;

    for(size_t i = 0; i < wired_count; i++)
    {
        if(!yyjson_mut_arr_add_str(
                    doc,
                    arr,
                    adb_wired_info_serial(wired[i])))
            goto cleanup;
    }

    for(size_t i = 0; i < wireless_count; i++)
    {
        memset(buffer, 0, sizeof(buffer));

        if(!adb_wireless_info_endpoint(
                    wireless[i],
                    buffer,
                    sizeof(buffer)))
            goto cleanup;

        if(!yyjson_mut_arr_add_str(doc, arr, buffer))
            goto cleanup;
    }

    if(!yyjson_mut_obj_add_val(doc, root, "devices", arr))
        goto cleanup;

    json = yyjson_mut_write(doc, 0, &json_len);
    if(!json)
        goto cleanup;

    res = adbc__send_json(client_fd, json, json_len);

cleanup:
    free(json);
    yyjson_mut_doc_free(doc);
    return res;
}

static bool adbc__handle_cmd(
        adbc_server_ctx_t *ctx,
        const int client_fd)
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
    size_t json_len = 0;
    yyjson_read_err read_err = {0};
    yyjson_doc *doc = NULL;
    yyjson_val *root = NULL;
    yyjson_val *cmd = NULL;
    yyjson_val *type = NULL;
    const char *cmd_str = NULL;

    if(!adbc__recv_json(client_fd, &json, &json_len))
        return false;

    doc = yyjson_read_opts(
            json,
            json_len,
            0,
            NULL,
            &read_err);

    if(!doc)
    {
        error("failed to parse json payload");
        info("reason: %s, at %zu byte",
                read_err.msg,
                read_err.pos);
        goto cleanup;
    }

    root = yyjson_doc_get_root(doc);
    if(!yyjson_is_obj(root))
    {
        error("malformed json payload");
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
        info("expected: \"cmd\", got: \"%s\"",
                yyjson_get_str(type));
        goto cleanup;
    }

    cmd = yyjson_obj_get(root, "cmd");
    if(!yyjson_is_str(cmd))
    {
        error("expected cmd to be a string");
        goto cleanup;
    }

    cmd_str = yyjson_get_str(cmd);
    for(size_t i = 0; i < ADBC_ARRSZ(handlers); i++)
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
static const char *adbc__log_level_strs[ADB__LOG_COUNT] =
{
    [ADB_LOG_DEBUG] = "debug",
    [ADB_LOG_INFO]  = "info",
    [ADB_LOG_WARN]  = "warn",
    [ADB_LOG_ERROR] = "error",
};


static void adbc__adb_log_callback(
        void *userdata,
        adb_log_level_t level,
        const char *msg)
{
    int fd = (int)(uintptr_t)userdata;
    yyjson_mut_doc *doc = NULL;
    yyjson_mut_val *root = NULL;
    char *json = NULL;
    size_t json_len = 0;

    if(fd < 0)
    {
        switch(level)
        {
            case ADB_LOG_DEBUG: debug("%s", msg); break;
            case ADB_LOG_INFO: info("%s", msg); break;
            case ADB_LOG_WARN: warn("%s", msg); break;
            case ADB_LOG_ERROR: error("%s", msg); break;
            case ADB__LOG_COUNT: 
            default: break;
        }
        return;
    }
    doc = yyjson_mut_doc_new(NULL);
    if(!doc)
        return;

    root = yyjson_mut_obj(doc);
    if(!root)
        goto cleanup;
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_obj_add_str(doc, root, "type", "log");
    yyjson_mut_obj_add_str(doc, root, 
            "level", adbc__log_level_strs[level]);
    yyjson_mut_obj_add_str(doc, root, "msg", msg);
    json = yyjson_mut_write(doc, 0, &json_len);
    if(!json)
        goto cleanup;

    adbc__send_json(fd, json, json_len);

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
    (void)arg;
    adbc_cmd_options_t *options = data;
    struct sockaddr_in address = {0};
    int fd = -1;
    int option = 1;
    adbc_server_ctx_t ctx = {0};

    info("starting server on port %d", ADBC_SERVER_PORT);
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0)
    {
        adbc_log_err_errno("failed to create a new socket");
        return;
    }

    if(setsockopt(
            fd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &option,
            sizeof(option)) != 0)
    {
        adbc_log_err_errno("failed to set socket options");
        goto cleanup;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(ADBC_SERVER_PORT);

    if(bind(
            fd,
            (const struct sockaddr *)&address,
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
            (void*)(uintptr_t)-1, options->level);
    adb_ctx_create(&ctx.ctx);
    while(!ctx.quit)
    {
        int client_fd = accept(fd, NULL, NULL);
        if(client_fd < 0)
            continue;

        adb_log_set_userdata((void*)(uintptr_t)client_fd);
        adbc__handle_cmd(&ctx, client_fd);
        adb_log_set_userdata((void*)(uintptr_t)-1);
        close(client_fd);
    }

    info("shutting down server on port %d", ADBC_SERVER_PORT);

cleanup:
    adb_ctx_destroy(ctx.ctx);
    close(fd);
}

static int adbc__connect_server(void)
{
    int fd = -1;
    struct sockaddr_in address = {0};

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0)
        return -1;

    address.sin_family = AF_INET;
    address.sin_port = htons(ADBC_SERVER_PORT);
    if(inet_pton(AF_INET, "127.0.0.1", 
                &address.sin_addr) != 1)
    {
        adbc_log_err_errno("failed to process network address");
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
    close(fd);
    return -1;
}

static void kill_server_cmd(
        const aparse_arg *arg, 
        void *data)
{
    static const char json[] = "{\"type\":\"cmd\",\"cmd\":\"quit\"}";
    (void)arg;
    (void)data;
    int fd = adbc__connect_server();
    adbc__send_json(fd, json, sizeof(json) - 1);
    close(fd);
}

static void devices_cmd(
        const aparse_arg *arg, 
        void *data)
{
    static const char cmd_json[] = "{\"type\":\"cmd\",\"cmd\":\"devices\"}";
    int server_fd = -1;

    (void)arg;
    (void)data;

    server_fd = adbc__connect_server();
    adbc__send_json(server_fd, cmd_json, sizeof(cmd_json) - 1);
    for(;;)
    {
        char *json = NULL;
        size_t json_len = 0;
        yyjson_read_err read_err = {0};
        yyjson_doc *doc = NULL;
        yyjson_val *root = NULL;
        yyjson_val *type = NULL;
        const char *type_str = NULL;

        adbc__recv_json(server_fd, &json, &json_len);
        
        doc = yyjson_read_opts(
                json,
                json_len,
                0,
                NULL,
                &read_err);

        if(!doc)
        {
            error("failed to parse json payload");
            info("reason: %s, at %zu byte",
                    read_err.msg,
                    read_err.pos);
            goto cleanup;
        }

        root = yyjson_doc_get_root(doc);
        if(!yyjson_is_obj(root))
        {
            error("malformed json payload");
            goto cleanup;
        }
        
        type = yyjson_obj_get(root, "type");
        if(!yyjson_is_str(type))
        {
            error("expected type to be a string");
            goto cleanup;
        }

        type_str = yyjson_get_str(type);
        if(strcmp(type_str, "log") == 0)
        {
            yyjson_val *level = yyjson_obj_get(root, "level");
            yyjson_val *msg = yyjson_obj_get(root, "msg");
            const char *level_str = yyjson_get_str(level);

            if(strcmp(level_str, "debug") == 0)
                debug("%s", yyjson_get_str(msg));
            else if(strcmp(level_str, "info") == 0)
                info("%s", yyjson_get_str(msg));
            else if(strcmp(level_str, "warn") == 0)
                warn("%s", yyjson_get_str(msg));
            else if(strcmp(level_str, "error") == 0)
                error("%s", yyjson_get_str(msg));
            
        } else if(strcmp(type_str, "result") == 0) {
            yyjson_val *devices = yyjson_obj_get(root, "devices");
            yyjson_val *item = NULL;
            size_t idx = 0;
            size_t max = 0;

            yyjson_arr_foreach(devices, idx, max, item)
                info("%s", yyjson_get_str(item));
            break;
        } else {
            error("unhandled packet type");
            break;
        }

cleanup:
        yyjson_doc_free(doc);
        free(json);
    }
    close(server_fd);
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

    if(aparse_parse(argc, argv, 
                main_args, &dispatch, 
                "libadb command line utils") != APARSE_STATUS_OK)
        return 1;

    if(level_str)
    {
        options.level = ADB__LOG_COUNT;
        for(size_t i = 0; i < ADBC_ARRSZ(adbc__log_level_strs); i++)
        {
            if(!strcmp(adbc__log_level_strs[i], level_str))
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
    } else
        options.level = ADB_LOG_ERROR;

    aparse_dispatch_all(&dispatch);
    return 0;
}
