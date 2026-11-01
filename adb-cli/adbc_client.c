
#include "adbc_client.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>

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

static int adbc__connect_server_once(bool report_connect_error)
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
        int error = errno;
        if(report_connect_error)
        {
            errno = error;
            adbc_log_err_errno("failed to connect to server");
        }
        goto cleanup;
    }

    return fd;

cleanup:
    if(close(fd) != 0)
        adbc_log_err_errno("failed to close client socket");
    return -1;
}

static int adbc__connect_server(adbc_cmd_options_t *options)
{
    adbc_cmd_options_t server_options = {0};
    struct timespec delay = {0, 100000000L};
    pid_t server_pid = -1;
    int fd = adbc__connect_server_once(false);

    if(fd >= 0)
        return fd;

    if(options != NULL)
        server_options = *options;

    server_pid = fork();
    if(server_pid < 0)
    {
        adbc_log_err_errno("failed to start server process");
        return adbc__connect_server_once(true);
    }

    if(server_pid == 0)
    {
        int null_fd = -1;

        if(setsid() < 0)
            _exit(EXIT_FAILURE);

        null_fd = open("/dev/null", O_RDWR);
        if(null_fd >= 0)
        {
            (void)dup2(null_fd, STDIN_FILENO);
            (void)dup2(null_fd, STDOUT_FILENO);
            (void)dup2(null_fd, STDERR_FILENO);
            if(null_fd > STDERR_FILENO)
                (void)close(null_fd);
        }

        start_server_cmd(&server_options);
        _exit(EXIT_FAILURE);
    }

    for(unsigned int attempt = 0U; attempt < 50U; attempt++)
    {
        int status = 0;
        pid_t waited = -1;

        fd = adbc__connect_server_once(false);
        if(fd >= 0)
        {
            ADBC_INFO("starting server on port %d", ADBC_SERVER_PORT);
            return fd;
        }

        waited = waitpid(server_pid, &status, WNOHANG);
        if(waited == server_pid || (waited < 0 && errno != EINTR))
            break;

        (void)nanosleep(&delay, NULL);
    }

    return adbc__connect_server_once(true);
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

static void adbc__format_mode(uint32_t mode, char output[11])
{
    output[0] = S_ISREG(mode) ? '-' :
        S_ISDIR(mode) ? 'd' :
        S_ISLNK(mode) ? 'l' :
        S_ISCHR(mode) ? 'c' :
        S_ISBLK(mode) ? 'b' :
        S_ISFIFO(mode) ? 'p' :
        S_ISSOCK(mode) ? 's' : '?';

    output[1] = (mode & S_IRUSR) ? 'r' : '-';
    output[2] = (mode & S_IWUSR) ? 'w' : '-';
    output[3] = (mode & S_ISUID) ?
        ((mode & S_IXUSR) ? 's' : 'S') :
        ((mode & S_IXUSR) ? 'x' : '-');
    output[4] = (mode & S_IRGRP) ? 'r' : '-';
    output[5] = (mode & S_IWGRP) ? 'w' : '-';
    output[6] = (mode & S_ISGID) ?
        ((mode & S_IXGRP) ? 's' : 'S') :
        ((mode & S_IXGRP) ? 'x' : '-');
    output[7] = (mode & S_IROTH) ? 'r' : '-';
    output[8] = (mode & S_IWOTH) ? 'w' : '-';
    output[9] = (mode & S_ISVTX) ?
        ((mode & S_IXOTH) ? 't' : 'T') :
        ((mode & S_IXOTH) ? 'x' : '-');
    output[10] = '\0';
}

static void adbc__format_timestamp(
        int64_t seconds,
        char *output,
        size_t output_size)
{
    time_t timestamp = (time_t)seconds;
    struct tm broken_down = {0};

    if(output_size == 0U)
        return;

    if((int64_t)timestamp != seconds ||
            localtime_r(&timestamp, &broken_down) == NULL ||
            strftime(output, output_size, "%Y-%m-%d %H:%M:%S %z",
                &broken_down) == 0U)
        (void)snprintf(output, output_size, "out of range");
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

static bool adbc__check_result_error(
        yyjson_val *root)
{
    yyjson_val *result = yyjson_obj_get(root, "result");
    if(yyjson_is_str(result) &&
            !strcmp(yyjson_get_str(result), "error"))
    {
        yyjson_val *error = yyjson_obj_get(root, "error");
        ADBC_ERROR("%s", yyjson_is_str(error) ? yyjson_get_str(error) :
                "transfer failed");
        return true;
    }

    return false;
}

static void adbc__display_transfer_progress(
        yyjson_val *root,
        uint64_t *last_bytes,
        uint64_t *last_time_ms)
{
    yyjson_val *bytes_val = yyjson_obj_get(root, "bytes");
    yyjson_val *total_val = yyjson_obj_get(root, "total_bytes");
    uint64_t bytes = 0U;
    uint64_t total = 0U;
    uint64_t now = adbc_util_monotonic_ms();
    uint64_t elapsed = now - *last_time_ms;
    uint64_t remaining = 0U;
    double rate = 0.0;
    double percent = 0.0;
    double eta_value = 0.0;
    uint64_t eta = 0U;
    unsigned int filled = 0U;

    if(!yyjson_is_uint(bytes_val) || !yyjson_is_uint(total_val))
    {
        ADBC_ERROR("malformed transfer progress packet");
        return;
    }

    bytes = yyjson_get_uint(bytes_val);
    total = yyjson_get_uint(total_val);
    if(elapsed > 0U && bytes >= *last_bytes)
        rate = (double)(bytes - *last_bytes) * 1000.0 / (double)elapsed;
    *last_bytes = bytes;
    *last_time_ms = now;

    if(total > 0U)
    {
        uint64_t shown_bytes = bytes < total ? bytes : total;
        percent = (double)shown_bytes * 100.0 / (double)total;
        remaining = total - shown_bytes;
    } else
        percent = 100.0;

    filled = (unsigned int)(percent * 20.0 / 100.0);
    if(rate > 0.0)
    {
        eta_value = (double)remaining / rate;
        eta = eta_value > 359999.0 ? 359999U : (uint64_t)eta_value;
    }

    fprintf(stderr, "\r[");
    for(unsigned int i = 0U; i < 20U; i++)
        fputc(i < filled ? '=' : ' ', stderr);
    fprintf(stderr, "] %3.0f%% %.1fKB/s ETA: %02llu:%02llus",
            percent, rate / 1024.0,
            (unsigned long long)(eta / 60U),
            (unsigned long long)(eta % 60U));
    fflush(stderr);
}

static void adbc__transfer_cmd(
    adbc_cmd_options_t *options,
    const char *command,
    const char *local_path,
    const char *remote_path)
{
    int fd = -1;
    yyjson_mut_doc *doc = NULL;
    yyjson_mut_val *root = NULL;
    bool done = false;
    bool displayed_progress = false;
    uint64_t last_bytes = 0U;
    uint64_t last_time_ms = adbc_util_monotonic_ms();

    fd = adbc__connect_server(options);
    if(fd < 0)
        goto cleanup;

    doc = yyjson_mut_doc_new(NULL);
    if(!doc)
    {
        ADBC_ERROR("failed to allocate transfer command");
        ADBC_INFO("reason: out of memory");
        goto cleanup;
    }

    root = yyjson_mut_obj(doc);
    if(!root)
    {
        ADBC_ERROR("failed to create transfer command");
        ADBC_INFO("reason: out of memory");
        goto cleanup;
    }

    yyjson_mut_doc_set_root(doc, root);
    if(!yyjson_mut_obj_add_str(doc, root, "type", "cmd") ||
       !yyjson_mut_obj_add_str(doc, root, "cmd", command) ||
    !yyjson_mut_obj_add_str(doc, root, "local_path", local_path) ||
    !yyjson_mut_obj_add_str(doc, root, "remote_path", remote_path))
    {
        ADBC_ERROR("failed to build transfer command");
        ADBC_INFO("reason: JSON allocation failed");
        goto cleanup;
    }

    if(!adbc_send_json(fd, doc))
    {
        ADBC_ERROR("failed to send transfer command");
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
            ADBC_ERROR("failed to receive transfer response");
            ADBC_INFO("reason: server disconnected or JSON read failed");
            yyjson_doc_free(response);
            break;
        }

        if(!adbc__get_packet_type(response, &response_root, &type))
        {
            yyjson_doc_free(response);
            break;
        }

        if(strcmp(type, "log") == 0)
            adbc_log_handle_server(response_root);
        else if(strcmp(type, "progress") == 0)
        {
            adbc__display_transfer_progress(response_root,
                    &last_bytes, &last_time_ms);
            displayed_progress = true;
        }
        else if(strcmp(type, "result") == 0)
        {
            if(displayed_progress)
            {
                fputc('\n', stderr);
                displayed_progress = false;
            }
            done = true;
            if(adbc__check_result_error(response_root))
                ADBC_ERROR("transfer command failed");
            else
                ADBC_INFO("transfer completed");
        }
        else
            ADBC_ERROR("unhandled server packet type: %s", type);

        yyjson_doc_free(response);
    }

    if(displayed_progress)
        fputc('\n', stderr);

cleanup:
    yyjson_mut_doc_free(doc);
    adbc__close_socket(&fd);
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

    fd = adbc__connect_server_once(true);
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
    adbc_cmd_options_t *options = data;
    int fd = -1;
    yyjson_mut_doc *doc = NULL;
    yyjson_mut_val *root = NULL;
    bool done = false;

    fd = adbc__connect_server(options);
    if(fd < 0)
        return;

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
       !yyjson_mut_obj_add_str(doc, root, "cmd", "devices") ||
       !yyjson_mut_obj_add_str(doc, root, "key_path", options->opt_key))
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
            ADBC_ERROR("failed to receive server response");
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
            yyjson_val *devices = yyjson_obj_get(response_root, "devices");
            yyjson_val *item = NULL;
            size_t index = 0U;
            size_t count = 0U;
            size_t device_count = 0U;

            if(!yyjson_is_arr(devices))
            {
                ADBC_ERROR("invalid devices response");
                ADBC_INFO("reason: expected devices to be an array");
                yyjson_doc_free(response);
                goto cleanup;
            }

            device_count = yyjson_arr_size(devices);
            ADBC_INFO("found %zu device%s", device_count,
                    device_count == 1U ? "" : "s");

            yyjson_arr_foreach(devices, index, count, item)
            {
                if(!yyjson_is_str(item))
                {
                    ADBC_ERROR("invalid device entry");
                    ADBC_INFO("reason: expected device entry to be a string");
                    yyjson_doc_free(response);
                    goto cleanup;
                }
                ADBC_INFO("%s", yyjson_get_str(item));
            }

            done = true;
        }
        else
            ADBC_ERROR("unhandled server packet type: %s", type);

        yyjson_doc_free(response);
    }

cleanup:
    adbc__close_socket(&fd);
}

void pair_cmd(void *data)
{
    adbc_cmd_options_t *options = data;
    char host[ADBC_IP_LENGTH_MAX] = {0};
    uint16_t port = 0U;
    adb_error_t res = ADB_ERR_OK;
    adb_ctx_t *ctx = NULL;
    adb_conn_t *conn = NULL;
    adb_key_t *key = NULL;

    adb_log_set(adbc_log_client_callback, NULL, options->opt_lvl);

    if(!adbc_util_parse_endpoint(
                options->endpoint, 
                host, sizeof(host), &port))
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

    fd = adbc__connect_server(options);
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
       !yyjson_mut_obj_add_str(doc, root, "serial", options->serial) ||
       !yyjson_mut_obj_add_str(doc, root, "key_path", options->opt_key))
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
            done = true;
            if(adbc__check_result_error(response_root))
                break;

            ADBC_INFO("connected to \"%s\" successfully", options->serial);
        }
        else
            ADBC_ERROR("unhandled server packet type: %s", type);

        yyjson_doc_free(response);
    }

cleanup:
    yyjson_mut_doc_free(doc);
    adbc__close_socket(&fd);
}

void disconnect_cmd(void *data)
{
    adbc_cmd_options_t *options = data;
    int fd = -1;
    yyjson_mut_doc *doc = NULL;
    yyjson_mut_val *root = NULL;
    bool done = false;

    fd = adbc__connect_server(options);
    if(fd < 0)
        goto cleanup;

    doc = yyjson_mut_doc_new(NULL);
    if(doc == NULL)
    {
        ADBC_ERROR("failed to allocate disconnect command");
        ADBC_INFO("reason: out of memory");
        goto cleanup;
    }

    root = yyjson_mut_obj(doc);
    if(root == NULL)
    {
        ADBC_ERROR("failed to create disconnect command");
        ADBC_INFO("reason: out of memory");
        goto cleanup;
    }

    yyjson_mut_doc_set_root(doc, root);
    if(!yyjson_mut_obj_add_str(doc, root, "type", "cmd") ||
       !yyjson_mut_obj_add_str(doc, root, "cmd", "disconnect") ||
       !yyjson_mut_obj_add_str(doc, root, "serial", options->serial))
    {
        ADBC_ERROR("failed to build disconnect command");
        ADBC_INFO("reason: JSON allocation failed");
        goto cleanup;
    }

    if(!adbc_send_json(fd, doc))
    {
        ADBC_ERROR("failed to send disconnect command");
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
            ADBC_ERROR("failed to receive disconnect response");
            ADBC_INFO("reason: server disconnected or JSON read failed");
            yyjson_doc_free(response);
            break;
        }

        if(!adbc__get_packet_type(response, &response_root, &type))
        {
            yyjson_doc_free(response);
            break;
        }

        if(strcmp(type, "log") == 0)
            adbc_log_handle_server(response_root);
        else if(strcmp(type, "result") == 0)
        {
            done = true;
            if(!adbc__check_result_error(response_root))
                ADBC_INFO("disconnected from \"%s\" successfully",
                        options->serial);
        }
        else
            ADBC_ERROR("unhandled server packet type: %s", type);

        yyjson_doc_free(response);
    }

cleanup:
    yyjson_mut_doc_free(doc);
    adbc__close_socket(&fd);
}

void push_cmd(void *data)
{
    adbc_cmd_options_t *options = data;
    adbc__transfer_cmd(options, "push",
            options->local_path, options->remote_path);
}

void pull_cmd(void *data)
{
    adbc_cmd_options_t *options = data;
    adbc__transfer_cmd(options, "pull",
            options->pull_local_path, options->pull_remote_path);
}

void stat_cmd(void *data)
{
    adbc_cmd_options_t *options = data;
    int fd = -1;
    yyjson_mut_doc *doc = NULL;
    yyjson_mut_val *root = NULL;
    bool done = false;

    fd = adbc__connect_server(options);
    if(fd < 0)
        goto cleanup;

    doc = yyjson_mut_doc_new(NULL);
    if(doc == NULL)
    {
        ADBC_ERROR("failed to allocate stat command");
        goto cleanup;
    }

    root = yyjson_mut_obj(doc);
    if(root == NULL)
    {
        ADBC_ERROR("failed to create stat command");
        goto cleanup;
    }

    yyjson_mut_doc_set_root(doc, root);
    if(!yyjson_mut_obj_add_str(doc, root, "type", "cmd") ||
            !yyjson_mut_obj_add_str(doc, root, "cmd", "stat") ||
            !yyjson_mut_obj_add_str(doc, root, "remote_path",
                options->stat_path))
    {
        ADBC_ERROR("failed to build stat command");
        goto cleanup;
    }

    if(!adbc_send_json(fd, doc))
    {
        ADBC_ERROR("failed to send stat command");
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
            ADBC_ERROR("failed to receive stat response");
            yyjson_doc_free(response);
            break;
        }

        if(!adbc__get_packet_type(response, &response_root, &type))
        {
            yyjson_doc_free(response);
            break;
        }

        if(strcmp(type, "log") == 0)
            adbc_log_handle_server(response_root);
        else if(strcmp(type, "result") == 0)
        {
            yyjson_val *dev = yyjson_obj_get(response_root, "dev");
            yyjson_val *ino = yyjson_obj_get(response_root, "ino");
            yyjson_val *mode = yyjson_obj_get(response_root, "mode");
            yyjson_val *nlink = yyjson_obj_get(response_root, "nlink");
            yyjson_val *uid = yyjson_obj_get(response_root, "uid");
            yyjson_val *gid = yyjson_obj_get(response_root, "gid");
            yyjson_val *size = yyjson_obj_get(response_root, "size");
            yyjson_val *atime = yyjson_obj_get(response_root, "atime");
            yyjson_val *mtime = yyjson_obj_get(response_root, "mtime");
            yyjson_val *ctime = yyjson_obj_get(response_root, "ctime");
            char mode_text[11] = {0};
            char atime_text[64] = {0};
            char mtime_text[64] = {0};
            char ctime_text[64] = {0};

            done = true;
            if(adbc__check_result_error(response_root))
            {
                yyjson_doc_free(response);
                break;
            }
            if(!yyjson_is_uint(dev) || !yyjson_is_uint(ino) ||
                    !yyjson_is_uint(mode) || !yyjson_is_uint(nlink) ||
                    !yyjson_is_uint(uid) || !yyjson_is_uint(gid) ||
                    !yyjson_is_uint(size) || !yyjson_is_int(atime) ||
                    !yyjson_is_int(mtime) || !yyjson_is_int(ctime))
            {
                ADBC_ERROR("invalid stat response");
                yyjson_doc_free(response);
                break;
            }

                adbc__format_mode((uint32_t)yyjson_get_uint(mode), mode_text);
                adbc__format_timestamp(yyjson_get_sint(atime),
                    atime_text, sizeof(atime_text));
                adbc__format_timestamp(yyjson_get_sint(mtime),
                    mtime_text, sizeof(mtime_text));
                adbc__format_timestamp(yyjson_get_sint(ctime),
                    ctime_text, sizeof(ctime_text));

            printf("Path: %s\n", options->stat_path);
            printf("Size: %llu bytes\n", (unsigned long long)
                    yyjson_get_uint(size));
                printf("Mode: %s (%#o)\n", mode_text,
                    (unsigned int)yyjson_get_uint(mode));
            printf("Uid: %u  Gid: %u\n",
                    (unsigned int)yyjson_get_uint(uid),
                    (unsigned int)yyjson_get_uint(gid));
            printf("Device: %llu  Inode: %llu  Links: %u\n",
                    (unsigned long long)yyjson_get_uint(dev),
                    (unsigned long long)yyjson_get_uint(ino),
                    (unsigned int)yyjson_get_uint(nlink));
                printf("Access: %s\n", atime_text);
                printf("Modify: %s\n", mtime_text);
                printf("Change: %s\n", ctime_text);
        }
        else
            ADBC_ERROR("unhandled server packet type: %s", type);

        yyjson_doc_free(response);
    }

cleanup:
    yyjson_mut_doc_free(doc);
    adbc__close_socket(&fd);
}
