
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
#include <sys/stat.h>

#include <yyjson.h>
#include <adb/adb.h>

#include "adb/adb_key.h"
#include "adbc_utils.h"
#include "adbc_log.h"
#include "adbc_io.h"
#include "adbc_client.h"
#include "adbc_dynarr.h"

typedef struct
{
    adb_conn_t *conn;
    char *serial;
} adbc__device_t;

typedef struct
{
    adb_ctx_t *adb_ctx;
    adbc_dynarr_t devices;
    int client_fd;
    bool quit;
} adbc__server_ctx_t;

static void adbc__device_destroy(adbc__device_t *device)
{
    if(device == NULL)
        return;

    adb_conn_destroy(device->conn);
    device->conn = NULL;
    free(device->serial);
    device->serial = NULL;
}

typedef struct
{
    adbc__server_ctx_t *server;
    FILE *file;
    uint64_t bytes;
    uint64_t total_bytes;
    uint64_t last_report_ms;
    uint64_t *batch_bytes;
    uint64_t *batch_total_bytes;
    bool sending;
} adbc__transfer_t;

typedef struct
{
    const char *local_path;
    const char *remote_path;
    FILE *file;
    adb_stream_t *stream;
    adbc__transfer_t transfer;
    bool local_created;
    bool success;
} adbc__pull_item_t;

typedef const char *adbc__server_error_t;

static bool adbc__send_progress(adbc__transfer_t *transfer)
{
    yyjson_mut_doc *doc = NULL;
    yyjson_mut_val *root = NULL;
    bool result = false;
    uint64_t bytes = 0U;
    uint64_t total_bytes = 0U;

    if(transfer == NULL || transfer->server == NULL ||
            transfer->server->client_fd < 0)
        return false;

    bytes = transfer->batch_bytes != NULL ?
        *transfer->batch_bytes : transfer->bytes;
    total_bytes = transfer->batch_total_bytes != NULL ?
        *transfer->batch_total_bytes : transfer->total_bytes;

    doc = yyjson_mut_doc_new(NULL);
    if(doc == NULL)
        return false;

    root = yyjson_mut_obj(doc);
    if(root == NULL)
        goto cleanup;

    yyjson_mut_doc_set_root(doc, root);
    if(!yyjson_mut_obj_add_str(doc, root, "type", "progress") ||
            !yyjson_mut_obj_add_uint(doc, root, "bytes", bytes) ||
            !yyjson_mut_obj_add_uint(doc, root, "total_bytes", total_bytes))
        goto cleanup;

    result = adbc_send_json(transfer->server->client_fd, doc);

cleanup:
    yyjson_mut_doc_free(doc);
    return result;
}

static bool adbc__maybe_send_progress(adbc__transfer_t *transfer)
{
    uint64_t now = adbc_util_monotonic_ms();
    if(now - transfer->last_report_ms < UINT64_C(1000))
        return true;

    if(!adbc__send_progress(transfer))
        return false;
    transfer->last_report_ms = now;
    return true;
}

static int adbc__transfer_read(void *userdata, uint8_t *buffer, size_t size)
{
    adbc__transfer_t *transfer = userdata;
    size_t count = fread(buffer, 1U, size, transfer->file);
    transfer->bytes += (uint64_t)count;
    if(transfer->batch_bytes != NULL)
        *transfer->batch_bytes += (uint64_t)count;
    if(!adbc__maybe_send_progress(transfer))
        return -(int)ADB_ERR_IO;
    return (int)count;
}

static int adbc__transfer_write(void *userdata, const uint8_t *buffer, size_t size)
{
    adbc__transfer_t *transfer = userdata;
    size_t count = fwrite(buffer, 1U, size, transfer->file);
    transfer->bytes += (uint64_t)count;
    if(transfer->batch_bytes != NULL)
        *transfer->batch_bytes += (uint64_t)count;
    if(!adbc__maybe_send_progress(transfer))
        return -(int)ADB_ERR_IO;
    return (int)count;
}

typedef adbc__server_error_t (*adbc__server_cmd_handler_fn)(
        adbc__server_ctx_t *ctx,
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
        adbc__server_ctx_t *ctx,
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
        adbc__server_ctx_t *ctx,
        yyjson_val *req_root,
        yyjson_mut_doc *response_doc,
        yyjson_mut_val *response_root)
{
    const char *error = NULL;
    const char *key_path = NULL;
    adb_key_t *key = NULL;
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

    error = adbc__json_get_str(req_root, "key_path", 
            false, &key_path);
    if(error != NULL)
        return error;
    if(key_path[0] == '\0')
        return "field 'key_path' must not be empty";

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

    adb_key_load(&key, ctx->adb_ctx, key_path);

    for(size_t i = 0U; i < wired_count; i++)
    {
        const char *serial = NULL;
        if(wired[i] == NULL ||
                (serial = adb_wired_info_serial(wired[i])) == NULL)
            return "failed to get wired device serial";

        if(!yyjson_mut_arr_add_strcpy(response_doc, arr, serial))
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
        if(!yyjson_mut_arr_add_strcpy(response_doc, arr, buffer))
            return "out of memory while adding wireless device";
    }

    if(!yyjson_mut_obj_add_val(response_doc, response_root, 
                "devices", arr))
        return "failed to add devices to JSON response";

    adb_key_destroy(key);
    return NULL;
}

static adbc__server_error_t adbc__cmd_connect(
        adbc__server_ctx_t *ctx,
        yyjson_val *req_root,
        yyjson_mut_doc *response_doc,
        yyjson_mut_val *response_root)
{
    adbc__server_error_t error = NULL;
    const char *serial = NULL;
    const char *key_path = NULL;
    char host[ADBC_IP_LENGTH_MAX] = {0};
    uint16_t port = 0U;
    adb_key_t *key = NULL;
    adb_error_t res = ADB_ERR_OK;
    adbc__device_t *device = NULL;
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
    if(key_path[0] == '\0')
        return "field 'key_path' must not be empty";

    res = adb_key_load(&key, ctx->adb_ctx, key_path);
    if(res != ADB_ERR_OK || key == NULL)
    {
        error = "failed to load ADB private key";
        goto cleanup;
    }

    for(size_t i = 0U; i < ctx->devices.size; i++)
    {
        adbc__device_t *candidate =
            &((adbc__device_t*)ctx->devices.data)[i];
        if(candidate->conn != NULL &&
                candidate->serial != NULL &&
                strcmp(candidate->serial, serial) == 0)
        {
            error = "device is already connected";
            goto cleanup;
        }
        if(device == NULL && candidate->conn == NULL)
        {
            device = candidate;
        }
    }

    if(device == NULL)
    {
        adbc__device_t empty = {0};
        if(!adbc_dynarr_push(&ctx->devices, &empty))
        {
            error = "out of memory while allocating device slot";
            goto cleanup;
        }

        device = &adbc_dynarr_get(&ctx->devices,
                adbc__device_t, ctx->devices.size - 1);
    }
    conn = &device->conn;

    if(adbc_util_parse_endpoint(serial, 
                host, sizeof(host), &port))
    {
        res = adb_conn_create_wireless(conn, 
                ctx->adb_ctx, host, port);
    } else {
        ADBC_WARN("serial is not a network endpoint, "
            "attempting to use as usb serial number");
        res = adb_conn_create_wired(conn,
                ctx->adb_ctx, serial);
    }

    if(res != ADB_ERR_OK)
    {
        error = adb_strerror(res);
        goto cleanup;
    }

    res = adb_handshake(*conn, key);
    if(res != ADB_ERR_OK)
    {
        error = adb_strerror(res);
        goto cleanup;
    }

    device->serial = malloc(strlen(serial) + 1U);
    if(device->serial == NULL)
    {
        error = "out of memory while storing device serial";
        goto cleanup;
    }
    memcpy(device->serial, serial, strlen(serial) + 1U);

cleanup:
    if(key != NULL)
        adb_key_destroy(key);

    if(error && device != NULL)
        adbc__device_destroy(device);
    return error;
}

static adbc__server_error_t adbc__cmd_disconnect(
        adbc__server_ctx_t *ctx,
        yyjson_val *req_root,
        yyjson_mut_doc *response_doc,
        yyjson_mut_val *response_root)
{
    const char *serial = NULL;
    adbc__server_error_t error = NULL;

    (void)response_doc;
    (void)response_root;

    if(ctx == NULL)
        return "invalid server context";

    error = adbc__json_get_str(req_root, "serial", true, &serial);
    if(error != NULL)
        return error;
    if(serial[0] == '\0')
        return "field 'serial' must not be empty";

    for(size_t i = 0U; i < ctx->devices.size; i++)
    {
        adbc__device_t *device =
            &adbc_dynarr_get(&ctx->devices, adbc__device_t, i);
        if(device->conn != NULL && device->serial != NULL &&
                strcmp(device->serial, serial) == 0)
        {
            adbc__device_destroy(device);
            return NULL;
        }
    }

    return "device is not connected";
}


static adbc__server_error_t adbc__cmd_push(
    adbc__server_ctx_t *ctx,
    yyjson_val *req_root,
    yyjson_mut_doc *response_doc,
    yyjson_mut_val *response_root)
{
    adb_conn_t **conn = NULL;
    adbc__device_t *device = NULL;
    const char *local_path = NULL;
    const char *remote_path = NULL;
    struct stat file_info = {0};
    FILE *file = NULL;
    adbc__transfer_t transfer = {0};
    adb_stream_t *stream = NULL;
    adb_error_t res = ADB_ERR_OK;
    adbc__server_error_t error = NULL;

    (void)response_doc;
    (void)response_root;

    if(ctx == NULL)
        return "invalid server context";

    for(size_t i = 0; i < ctx->devices.size; i++)
    {
        adbc__device_t *candidate =
            &adbc_dynarr_get(&ctx->devices, adbc__device_t, i);
        if(candidate->conn == NULL)
            continue;
        if(conn)
            return "more than one device was connected";
        device = candidate;
        conn = &device->conn;
    }

    if (!conn)
        return "no device was connected";

    error = adbc__json_get_str(
        req_root, "local_path", true, &local_path);
    if (error)
        goto end;

    error = adbc__json_get_str(
        req_root, "remote_path", true, &remote_path);
    if (error)
        goto end;

    if(stat(local_path, &file_info) != 0 || file_info.st_size < 0)
    {
        error = "failed to get local file information";
        goto end;
    }

    file = fopen(local_path, "rb");
    if(file == NULL)
    {
        error = "failed to open local file";
        goto end;
    }

    transfer.server = ctx;
    transfer.file = file;
    transfer.total_bytes = (uint64_t)file_info.st_size;
    transfer.last_report_ms = adbc_util_monotonic_ms();
    res = adb_sync_push_start(&stream, *conn, remote_path,
            (uint32_t)file_info.st_mode,
            (uint32_t)file_info.st_mtim.tv_sec,
            (size_t)file_info.st_size,
            adbc__transfer_read, &transfer);
    while(res == ADB_ERR_OK && stream != NULL && !adb_stream_done(stream))
    {
        res = adb_conn_stream_dispatch(*conn);
        if(res == ADB_ERR_NOT_FOUND)
            res = ADB_ERR_OK;
        else if(res != ADB_ERR_OK && !adb_stream_done(stream))
            break;
    }
    if(stream != NULL)
    {
        adb_error_t finalize_res = adb_sync_push_finalize(stream);
        stream = NULL;
        if(res == ADB_ERR_OK)
            res = finalize_res;
    }
    if (res != ADB_ERR_OK)
        error = adb_strerror(res);
    else if(!adbc__send_progress(&transfer))
        error = "failed to send final transfer progress";

end:
    if(file != NULL)
        (void)fclose(file);
    if(error)
        adbc__device_destroy(device);
    return error;
}

static adbc__server_error_t adbc__cmd_pull(
        adbc__server_ctx_t *ctx,
        yyjson_val *req_root,
        yyjson_mut_doc *response_doc,
        yyjson_mut_val *response_root)
{
    adb_conn_t **conn = NULL;
    adbc__device_t *device = NULL;
    yyjson_val *files = NULL;
    adbc__pull_item_t *items = NULL;
    size_t file_count = 0U;
    size_t active_streams = 0U;
    uint64_t batch_bytes = 0U;
    uint64_t batch_total_bytes = 0U;
    adb_error_t dispatch_res = ADB_ERR_OK;
    adbc__server_error_t error = NULL;

    (void)response_doc;
    (void)response_root;

    if(ctx == NULL)
        return "invalid server context";

    for(size_t i = 0; i < ctx->devices.size; i++)
    {
        adbc__device_t *candidate =
            &adbc_dynarr_get(&ctx->devices, adbc__device_t, i);
        if(candidate->conn == NULL)
            continue;
        if(conn)
            return "more than one device was connected";
        device = candidate;
        conn = &device->conn;
    }

    if(!conn)
        return "no device was connected";

    files = yyjson_obj_get(req_root, "files");
    if(!yyjson_is_arr(files))
        return "field 'files' must be an array";
    file_count = yyjson_arr_size(files);
    if(file_count == 0U)
        return "pull requires at least one file";

    items = calloc(file_count, sizeof(*items));
    if(items == NULL)
        return "failed to allocate pull file list";

    for(size_t i = 0U; i < file_count; i++)
    {
        yyjson_val *file = yyjson_arr_get(files, i);

        error = adbc__json_get_str(file, "remote_path", true,
                &items[i].remote_path);
        if(error != NULL)
            goto cleanup;
        error = adbc__json_get_str(file, "local_path", true,
                &items[i].local_path);
        if(error != NULL)
            goto cleanup;

        for(size_t previous = 0U; previous < i; previous++)
        {
            if(strcmp(items[previous].local_path, items[i].local_path) == 0)
            {
                error = "pull destination paths must be unique";
                goto cleanup;
            }
        }

        items[i].file = fopen(items[i].local_path, "wb");
        if(items[i].file == NULL)
        {
            error = "failed to open local file";
            goto cleanup;
        }
        items[i].local_created = true;
        items[i].transfer.server = ctx;
        items[i].transfer.file = items[i].file;
        items[i].transfer.last_report_ms = adbc_util_monotonic_ms();
        items[i].transfer.batch_bytes = &batch_bytes;
        items[i].transfer.batch_total_bytes = &batch_total_bytes;
    }

    for(size_t i = 0U; i < file_count; i++)
    {
        adb_error_t res = adb_sync_pull_start(&items[i].stream, *conn,
                items[i].remote_path, adbc__transfer_write,
                &items[i].transfer);
        if(res != ADB_ERR_OK)
        {
            error = adb_strerror(res);
            goto cleanup;
        }
        active_streams++;
    }

    while(active_streams > 0U)
    {
        dispatch_res = adb_conn_stream_dispatch(*conn);
        for(size_t i = 0U; i < file_count; i++)
        {
            adb_error_t res = ADB_ERR_OK;
            if(items[i].stream == NULL ||
                    !adb_stream_done(items[i].stream))
                continue;

            adb_stream_close(items[i].stream);
            items[i].stream = NULL;
            active_streams--;
            if(res == ADB_ERR_OK)
                items[i].success = true;
        }

        if(dispatch_res != ADB_ERR_OK &&
                dispatch_res != ADB_ERR_NOT_FOUND && error == NULL)
            error = adb_strerror(dispatch_res);
    }

    if(error == NULL && !adbc__send_progress(&items[file_count - 1U].transfer))
        error = "failed to send final transfer progress";

cleanup:
    for(size_t i = 0U; i < file_count; i++)
    {
        if(items[i].stream != NULL)
        {
            (void)adb_stream_close(items[i].stream);
            items[i].stream = NULL;
        }
        if(items[i].file != NULL)
        {
            if(fclose(items[i].file) != 0)
            {
                items[i].success = false;
                if(error == NULL)
                    error = "failed to close local file";
            }
            items[i].file = NULL;
        }
        if(items[i].local_created && !items[i].success)
            (void)remove(items[i].local_path);
    }
    free(items);

    if(error)
        adbc__device_destroy(device);
    return error;
}

static adbc__server_error_t adbc__cmd_stat(
    adbc__server_ctx_t *ctx,
    yyjson_val *req_root,
    yyjson_mut_doc *response_doc,
    yyjson_mut_val *response_root)
{
    adb_conn_t *conn = NULL;
    const char *remote_path = NULL;
    adb_sync_stat_t remote_info = {0};
    adb_error_t res = ADB_ERR_OK;

    if(ctx == NULL || response_doc == NULL || response_root == NULL)
        return "invalid stat command arguments";

    for(size_t i = 0U; i < ctx->devices.size; i++)
    {
        adbc__device_t *candidate =
            &adbc_dynarr_get(&ctx->devices, adbc__device_t, i);
        if(candidate->conn == NULL)
            continue;
        if(conn != NULL)
            return "more than one device was connected";
        conn = candidate->conn;
    }

    if(conn == NULL)
        return "no device was connected";

    if(adbc__json_get_str(req_root, "remote_path", true,
                &remote_path) != NULL)
        return "invalid remote path";

    res = adb_sync_stat(conn, remote_path, &remote_info, NULL);
    if(res != ADB_ERR_OK)
        return adb_strerror(res);

    if(!yyjson_mut_obj_add_uint(response_doc, response_root,
                "dev", remote_info.dev) ||
            !yyjson_mut_obj_add_uint(response_doc, response_root,
                "ino", remote_info.ino) ||
            !yyjson_mut_obj_add_uint(response_doc, response_root,
                "mode", remote_info.mode) ||
            !yyjson_mut_obj_add_uint(response_doc, response_root,
                "nlink", remote_info.nlink) ||
            !yyjson_mut_obj_add_uint(response_doc, response_root,
                "uid", remote_info.uid) ||
            !yyjson_mut_obj_add_uint(response_doc, response_root,
                "gid", remote_info.gid) ||
            !yyjson_mut_obj_add_uint(response_doc, response_root,
                "size", remote_info.size) ||
            !yyjson_mut_obj_add_sint(response_doc, response_root,
                "atime", remote_info.atime) ||
            !yyjson_mut_obj_add_sint(response_doc, response_root,
                "mtime", remote_info.mtime) ||
            !yyjson_mut_obj_add_sint(response_doc, response_root,
                "ctime", remote_info.ctime))
        return "failed to construct stat response";

    return NULL;
}

typedef struct
{
    yyjson_mut_doc *doc;
    yyjson_mut_val *entries;
} adbc__ls_response_t;

static adb_error_t adbc__append_ls_entry(
        void *userdata,
        const char *name,
        const adb_sync_stat_t *info,
        const int error_num)
{
    adbc__ls_response_t *response = userdata;
    yyjson_mut_val *entry = NULL;

    if(response == NULL || response->doc == NULL ||
            response->entries == NULL || name == NULL || info == NULL)
        return ADB_ERR_PARAM;

    entry = yyjson_mut_obj(response->doc);
    if(entry == NULL ||
            !yyjson_mut_obj_add_strcpy(response->doc, entry, "name", name) ||
            !yyjson_mut_obj_add_uint(response->doc, entry, "dev", info->dev) ||
            !yyjson_mut_obj_add_uint(response->doc, entry, "ino", info->ino) ||
            !yyjson_mut_obj_add_uint(response->doc, entry, "mode", info->mode) ||
            !yyjson_mut_obj_add_uint(response->doc, entry, "nlink", info->nlink) ||
            !yyjson_mut_obj_add_uint(response->doc, entry, "uid", info->uid) ||
            !yyjson_mut_obj_add_uint(response->doc, entry, "gid", info->gid) ||
            !yyjson_mut_obj_add_uint(response->doc, entry, "size", info->size) ||
            !yyjson_mut_obj_add_sint(response->doc, entry, "atime", info->atime) ||
            !yyjson_mut_obj_add_sint(response->doc, entry, "mtime", info->mtime) ||
            !yyjson_mut_obj_add_sint(response->doc, entry, "ctime", info->ctime) ||
            !yyjson_mut_obj_add_sint(response->doc, entry, "errno", error_num) ||
            !yyjson_mut_arr_add_val(response->entries, entry))
        return ADB_ERR_NO_MEM;

    return ADB_ERR_OK;
}

static adbc__server_error_t adbc__cmd_ls(
        adbc__server_ctx_t *ctx,
        yyjson_val *req_root,
        yyjson_mut_doc *response_doc,
        yyjson_mut_val *response_root)
{
    adb_conn_t *conn = NULL;
    const char *remote_path = NULL;
    yyjson_mut_val *entries = NULL;
    adbc__ls_response_t response = {0};
    adb_error_t res = ADB_ERR_OK;

    if(ctx == NULL || response_doc == NULL || response_root == NULL)
        return "invalid ls command arguments";

    for(size_t i = 0U; i < ctx->devices.size; i++)
    {
        adbc__device_t *candidate =
            &adbc_dynarr_get(&ctx->devices, adbc__device_t, i);
        if(candidate->conn == NULL)
            continue;
        if(conn != NULL)
            return "more than one device was connected";
        conn = candidate->conn;
    }

    if(conn == NULL)
        return "no device was connected";

    if(adbc__json_get_str(req_root, "remote_path", true,
                &remote_path) != NULL)
        return "invalid remote path";

    entries = yyjson_mut_arr(response_doc);
    if(entries == NULL)
        return "out of memory while creating directory listing";

    response.doc = response_doc;
    response.entries = entries;
    res = adb_sync_list(conn, remote_path, adbc__append_ls_entry, &response);
    if(res != ADB_ERR_OK)
        return adb_strerror(res);

    if(!yyjson_mut_obj_add_val(response_doc, response_root,
                "entries", entries))
        return "failed to add directory entries to JSON response";

    return NULL;
}

static bool adbc__handle_cmd(adbc__server_ctx_t *ctx, const int client_fd)
{
    static const struct
    {
        const char *cmd;
        adbc__server_cmd_handler_fn handler;
    } handlers[] =
    {
        { "quit", adbc__cmd_quit },
        { "devices", adbc__cmd_devices },
        { "connect", adbc__cmd_connect },
        { "disconnect", adbc__cmd_disconnect },
        { "push", adbc__cmd_push },
        { "pull", adbc__cmd_pull },
        { "stat", adbc__cmd_stat },
        { "ls", adbc__cmd_ls }
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
        ADBC_ERROR("malformed server JSON was recieved");
        error = "expected a valid JSON object";
        goto respond;
    }

    error = adbc__json_get_str(req_root, "type", 
            true, &type);
    if(error != NULL)
        goto respond;

    if(strcmp(type, "cmd") != 0)
    {
        ADBC_ERROR("unexpected packet type: \"%s\"", type);
        error = "unexpected packet type was recieved";
        goto respond;
    }

    error = adbc__json_get_str(req_root, "cmd", 
            true, &cmd);
    if(error != NULL)
        goto respond;

    ADBC_INFO("recieved command \"%s\" from client", cmd);
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
        ADBC_ERROR("unknown command %s was recieved", cmd);
        error = "unknown command was recieved";
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
    } else
        result = true;

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
    adbc__server_ctx_t ctx = {0};
    int fd = -1;
    int client_fd = -1;
    int option = 1;
    adbc__device_t device = {0};

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

    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, 
                &option, sizeof(option)) != 0)
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

    adbc_dynarr_init(&ctx.devices, sizeof(adbc__device_t));
    while(!ctx.quit)
    {
        ADBC_INFO("waiting for the next command");
        client_fd = accept(fd, NULL, NULL);
        if(client_fd < 0)
        {
            if(errno == EINTR)
                continue;

            adbc_log_err_errno("failed to accept client");
            break;
        }

        adb_log_set_userdata((void *)(intptr_t)client_fd);
        ctx.client_fd = client_fd;
        if(!adbc__handle_cmd(&ctx, client_fd))
            ADBC_WARN("failed to handle client command");
        ctx.client_fd = -1;
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

    adbc_dynarr_foreach(&ctx.devices, adbc__device_t, device)
        adbc__device_destroy(&device);
    adbc_dynarr_destroy(&ctx.devices);

    if(ctx.adb_ctx != NULL)
        adb_ctx_destroy(ctx.adb_ctx);
    if(fd >= 0 && close(fd) != 0)
        adbc_log_err_errno("failed to close server socket");
}
